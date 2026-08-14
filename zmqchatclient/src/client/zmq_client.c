#include "zmq_client.h"
#include "zmq_client_req.h"
#include "zmq_client_sub.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <zmq.h>

static volatile sig_atomic_t running = 1;

static void stop_running(int e)
{
    printf("\n--- ZMQ :: INPROC :: SIGNAL HANDLER ---\n");
    running = false;
    printf("running -> false\n");
}

int zmq_client_run(void)
{
    signal(SIGINT, stop_running);
    char username[64];
    printf("enter login: ");
    if (scanf("%63s", username) != 1)
        return 1;

    printf("\nDostępne komendy:\n");
    printf("  /rooms                     - wyświetla listę pokojów na serwerze\n");
    printf("  /join <nazwa_pokoju>       - dołącza do pokoju\n");
    printf("  /leave <nazwa_pokoju>      - opuszcza pokój\n");
    printf("  /msg <nazwa_pokoju> <treść>- wysyła wiadomość do pokoju\n");
    printf("  /dm <użytkownik> <treść>   - wysyła wiadomość prywatną\n");
    printf("  /quit                      - wyjście z programu\n\n");

    void* context = zmq_ctx_new();

    pthread_t       sub_thread;
    SubscriberArgs* sub_args = malloc(sizeof(SubscriberArgs));
    sub_args->context        = context;
    sub_args->running        = &running;
    strncpy(sub_args->username, username, sizeof(sub_args->username));
    pthread_create(&sub_thread, NULL, subscriber_routine, sub_args);

    pthread_t      req_thread;
    RequesterArgs* req_args = malloc(sizeof(RequesterArgs));
    req_args->context       = context;
    req_args->running       = &running;
    strncpy(req_args->username, username, sizeof(req_args->username));
    pthread_create(&req_thread, NULL, requester_routine, req_args);

    // Czekamy na sygnał (pętla sprawdzająca stan flagi running)
    while (running)
    {
        sleep(1);  // 1s uśpienia wątku głównego, by nie obciążać CPU
    }

    // KLUCZ: Najpierw zamykamy kontekst, co natychmiast wybudzi zmq_msg_recv w subskrybencie!
    printf("[ZMQClient] closing context...\n");
    zmq_ctx_term(context);

    printf("[ZMQClient] context closed...\n");
    pthread_join(sub_thread, NULL);
    pthread_join(req_thread, NULL);

    return EXIT_SUCCESS;
}
