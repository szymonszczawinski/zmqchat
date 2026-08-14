#include "zmq_client_ui.h"
#include "zmq_client_common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <zmq.h>
#include <poll.h>

static void clean_up(void* ctrl_pub, UiArgs* args);

void* ui_routine(void* arg)
{
    printf("[ZMQClient][UI Thread] start ui_routine\n");

    UiArgs*                args    = (UiArgs*)arg;
    void*                  context = args->context;
    volatile sig_atomic_t* running = args->running;

    // 1. Gniazdo do wysyłania komend do wątku REQ
    void* command_socket = zmq_socket(context, ZMQ_PAIR);
    if (zmq_connect(command_socket, COMMAND_SOCKET_ADDRESS) != 0)
    {
        perror("[UI Thread] connect inproc://command error");
        free(args);
        return NULL;
    }

    // 2. Gniazdo do odbierania wiadomości z wątku SUB
    void* ui_notif_socket = zmq_socket(context, ZMQ_PAIR);
    if (zmq_bind(ui_notif_socket, UI_NOTIF_ADDRESS) != 0)
    {
        perror("[UI Thread] bind inproc://ui-notifications error");
        zmq_close(command_socket);
        free(args);
        return NULL;
    }

    printf("\nDostępne komendy:\n");
    printf("  /rooms                     - lista pokojów\n");
    printf("  /join <nazwa_pokoju>       - dołącza do pokoju\n");
    printf("  /leave <nazwa_pokoju>      - opuszcza pokój\n");
    printf("  /msg <nazwa_pokoju> <treść>- wysyła wiadomość\n");
    printf("  /dm <użytkownik> <treść>   - wiadomość prywatna\n");
    printf("  /quit                      - wyjście\n\n");

    struct pollfd std_pfd;
    std_pfd.fd     = STDIN_FILENO;
    std_pfd.events = POLLIN;

    char line[512];

    while (running && *running)
    {
        printf("> ");
        fflush(stdout);

        // A. Sprawdzamy, czy przyszła wiadomość z wątku SUB (non-blocking recv)
        char notif_buf[1024];
        int  notif_bytes = zmq_recv(ui_notif_socket, notif_buf, sizeof(notif_buf) - 1, ZMQ_DONTWAIT);
        if (notif_bytes > 0)
        {
            notif_buf[notif_bytes] = '\0';
            printf("\r%s\n> ", notif_buf);  // \r czyści bieżący znak zachowując kursor
            fflush(stdout);
        }

        // B. Sprawdzamy wejście z klawiatury (100 ms timeout)
        int poll_res = poll(&std_pfd, 1, 100);
        if (poll_res <= 0)
        {
            continue;
        }

        if (!fgets(line, sizeof(line), stdin))
        {
            break;
        }

        line[strcspn(line, "\r\n")] = 0;
        if (strlen(line) == 0)
        {
            continue;
        }

        if (strcmp(line, "/quit") == 0)
        {
            *running = 0;
            zmq_send(command_socket, "/quit", 5, 0);
            break;
        }

        // Wysyłamy komendę do REQ i czekamy na Odpowiedź (ACK/ERR)
        zmq_send(command_socket, line, strlen(line), 0);

        char response_buf[2048];
        int  bytes = zmq_recv(command_socket, response_buf, sizeof(response_buf) - 1, 0);
        if (bytes > 0)
        {
            response_buf[bytes] = '\0';
            printf("%s\n", response_buf);
        }
    }

    zmq_close(command_socket);
    zmq_close(ui_notif_socket);
    free(args);
    printf("[ZMQClient][UI Thread] exit\n");
    return NULL;
}

static void clean_up(void* command_socket, UiArgs* args)

{
    if (command_socket)
    {
        zmq_close(command_socket);
    }
    if (args)
    {
        free(args);
    }
}
