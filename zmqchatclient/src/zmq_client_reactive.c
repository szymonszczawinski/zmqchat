#include <zmq.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>

// ============================================================================
// WĄTEK SERWERA (ZMQ ROUTER)
// Symuluje zewnętrzny serwer sieciowy (np. na tcp://localhost:5555)
// ============================================================================
static void* serwer_router_worker(void* ctx)
{
    void* router = zmq_socket(ctx, ZMQ_ROUTER);
    zmq_bind(router, "tcp://*:5555");

    printf("[SERWER] Uruchomiony na tcp://*:5555\n");

    int msg_count = 0;
    while (msg_count < 2)
    {  // Obsłużymy 2 zapytania i kończymy
        char identity[256];
        char delimiter[10];
        char request[256];

        // Gniazdo ROUTER odbiera wiadomości multipart w formacie:
        // Frame 1: Identyfikator nadawcy (DEALERa)
        // Frame 2: Pusty delimiter
        // Frame 3: Treść zapytania
        int id_len       = zmq_recv(router, identity, sizeof(identity) - 1, 0);
        identity[id_len] = '\0';

        zmq_recv(router, delimiter, sizeof(delimiter), 0);

        int req_len      = zmq_recv(router, request, sizeof(request) - 1, 0);
        request[req_len] = '\0';

        printf("  [SERWER] Odebrano od '%s': '%s'\n", identity, request);

        // Symulujemy czas przetwarzania zapytania po stronie serwera
        sleep(1);

        // Przygotowanie odpowiedzi
        char response[256];
        snprintf(response, sizeof(response), "Odpowiedź na [%s]", request);

        printf("  [SERWER] Odsyłam do '%s': '%s'\n", identity, response);

        // Odsyłanie odpowiedzi przez ROUTER:
        // Frame 1: Identyfikator docelowego klienta
        // Frame 2: Pusty delimiter
        // Frame 3: Treść odpowiedzi
        zmq_send(router, identity, id_len, ZMQ_SNDMORE);
        zmq_send(router, "", 0, ZMQ_SNDMORE);
        zmq_send(router, response, strlen(response), 0);

        msg_count++;
    }

    zmq_close(router);
    printf("[SERWER] Zakończył pracę.\n");
    return NULL;
}

// ============================================================================
// WĄTEK B (Komponent B: Obsługuje sieciowego DEALERa i pętlę zdarzeń)
// ============================================================================
static void* komponent_B_worker(void* ctx)
{
    // 1. Połączenie SIECIOWE do Serwera ROUTER
    void*       dealer   = zmq_socket(ctx, ZMQ_DEALER);
    const char* identity = "Klient-Reaktywny";
    zmq_setsockopt(dealer, ZMQ_IDENTITY, identity, strlen(identity));
    zmq_connect(dealer, "tcp://localhost:5555");

    // 2. Połączenie WEWNĘTRZNE (PAIR do komunikacji z Wątkiem A)
    void* inproc_sock = zmq_socket(ctx, ZMQ_PAIR);
    zmq_bind(inproc_sock, "inproc://watek_AB");

    zmq_pollitem_t items[] = {
        { dealer, 0, ZMQ_POLLIN, 0 },      // Zdarzenia z SIECI (od SERWERA)
        { inproc_sock, 0, ZMQ_POLLIN, 0 }  // Zdarzenia LOKALNE (od Wątku A)
    };

    printf("[Wątek B] Pętla zdarzeń uruchomiona...\n");

    int processed = 0;
    while (processed < 4)
    {  // 2 wysłania + 2 odbiory = 4 zdarzenia
        // Reaktywne wybudzenie: blokuje bez zużycia CPU (-1) do nadejścia zdarzenia
        int rc = zmq_poll(items, 2, -1);
        if (rc == -1)
            break;

        // SCENARIUSZ 1: Przyszła odpowiedź z sieci od Serwera -> Przekaż do Wątku A
        if (items[0].revents & ZMQ_POLLIN)
        {
            char buffer[256];
            zmq_recv(dealer, buffer, sizeof(buffer), 0);  // Odrzucamy delimiter
            int bytes = zmq_recv(dealer, buffer, sizeof(buffer) - 1, 0);

            if (bytes > 0)
            {
                buffer[bytes] = '\0';
                printf("[Wątek B] <- Odebrano z SIECI: '%s' -> Przekazuję do Wątku A\n", buffer);

                // Odsyłamy do Wątku A przez inproc
                zmq_send(inproc_sock, buffer, bytes, 0);
                processed++;
            }
        }

        // SCENARIUSZ 2: Wątek A chce wysłać wiadomość w sieć -> Przekaż do Serwera
        if (items[1].revents & ZMQ_POLLIN)
        {
            char command[256];
            int  bytes = zmq_recv(inproc_sock, command, sizeof(command) - 1, 0);

            if (bytes > 0)
            {
                command[bytes] = '\0';
                printf("[Wątek B] -> Zlecenie od Wątku A: '%s' -> Wysyłam w SIEĆ\n", command);

                // Format dla DEALERa rozmawiającego z ROUTERem (Delimiter + Payload)
                zmq_send(dealer, "", 0, ZMQ_SNDMORE);
                zmq_send(dealer, command, bytes, 0);
                processed++;
            }
        }
    }

    zmq_close(dealer);
    zmq_close(inproc_sock);
    printf("[Wątek B] Zakończył pracę.\n");
    return NULL;
}

// ============================================================================
// WĄTEK A (Komponent A: Logika aplikacyjna / biznesowa)
// ============================================================================
static void* komponent_A_worker(void* ctx)
{
    void* inproc_sock = zmq_socket(ctx, ZMQ_PAIR);
    zmq_connect(inproc_sock, "inproc://watek_AB");

    zmq_pollitem_t items[] = { { inproc_sock, 0, ZMQ_POLLIN, 0 } };

    sleep(1);  // Dajemy czas na zestawienie połączeń

    // --- ZAPYTANIE 1 ---
    printf("\n[Wątek A] *** Zlecam Zapytanie #1 ***\n");
    zmq_send(inproc_sock, "Zapytanie #1", 12, 0);

    // Symulacja nieblokującego sprawdzania wyników podczas robienia innych rzeczy
    for (int i = 0; i < 3; i++)
    {
        sleep(1);
        printf("[Wątek A] Przetwarzam inną logikę biznesową...\n");

        // Sprawdzamy nieblokująco (timeout = 0), czy odpowiedź już czeka
        if (zmq_poll(items, 1, 0) > 0 && (items[0].revents & ZMQ_POLLIN))
        {
            char response[256];
            int  bytes = zmq_recv(inproc_sock, response, sizeof(response) - 1, 0);
            if (bytes > 0)
            {
                response[bytes] = '\0';
                printf("[Wątek A] >>> ODEBRAŁEM ODPOWIEDŹ: '%s' <<<\n", response);
            }
        }
    }

    // --- ZAPYTANIE 2 ---
    printf("\n[Wątek A] *** Zlecam Zapytanie #2 ***\n");
    zmq_send(inproc_sock, "Zapytanie #2", 12, 0);

    // Oczekiwanie blokujące (z czasem timeout do 3000 ms)
    if (zmq_poll(items, 1, 3000) > 0)
    {
        char response[256];
        int  bytes = zmq_recv(inproc_sock, response, sizeof(response) - 1, 0);
        if (bytes > 0)
        {
            response[bytes] = '\0';
            printf("[Wątek A] >>> ODEBRAŁEM ODPOWIEDŹ: '%s' <<<\n", response);
        }
    }

    zmq_close(inproc_sock);
    printf("[Wątek A] Zakończył pracę.\n");
    return NULL;
}

// ============================================================================
// MAIN
// ============================================================================
int zmq_client_reactive_run(void)
{
    void* ctx = zmq_ctx_new();

    pthread_t thread_server, thread_B, thread_A;

    // Uruchamiamy wszystkie 3 wątki
    pthread_create(&thread_server, NULL, serwer_router_worker, ctx);
    pthread_create(&thread_B, NULL, komponent_B_worker, ctx);
    pthread_create(&thread_A, NULL, komponent_A_worker, ctx);

    // Czekamy na zakończenie wątków
    pthread_join(thread_A, NULL);
    pthread_join(thread_B, NULL);
    pthread_join(thread_server, NULL);

    zmq_ctx_destroy(ctx);
    printf("\nProgram zakończył działanie pomyślnie.\n");
    return 0;
}
