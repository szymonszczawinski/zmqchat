#include "chat_client_ui.h"
#include "chat_client_common.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <zmq.h>
#include <poll.h>

static void clean_up(void* socket_sub_shutdown, void* socket_pair_command, void* socket_pair_ui);
static void print_received_chat_message(char* chat_message);

void* routine_chat_ui(void* arg)
{
    printf("[ZMQClient][UI Thread] start ui_routine\n");

    UiArgs* args    = (UiArgs*)arg;
    void*   context = args->context;

    // Control socket INPROC (SUB for kill/stop signal)
    void* socket_sub_shutdown = zmq_socket(context, ZMQ_SUB);
    if (zmq_connect(socket_sub_shutdown, INPROC_SHUTDOWN_ADDR) == -1)
    {
        printf("[ZMQClient][UI Thread] shutdown socket connect failed: %s\n", strerror(errno));
        return NULL;
    }

    // 1. Gniazdo do wysyłania komend do wątku REQ
    void* socket_pair_command = zmq_socket(context, ZMQ_PAIR);
    if (zmq_connect(socket_pair_command, CONROLLER_UI_COMMAND_ADDRESS) != 0)
    {
        perror("[ZMQClient][UI Thread] connect inproc://command error");
        clean_up(socket_sub_shutdown, socket_pair_command, NULL);
        return NULL;
    }

    // 2. Gniazdo do odbierania wiadomości z wątku SUB
    void* socket_pair_sub = zmq_socket(context, ZMQ_PAIR);
    if (zmq_bind(socket_pair_sub, CONTROLLER_UI_NOTIFICATION_ADDRESS) != 0)
    {
        perror("[ZMQClient][UI Thread] bind inproc://ui-notifications error");
        clean_up(socket_sub_shutdown, socket_pair_command, socket_pair_sub);
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

    // TODO: add shutdown handing
    while (1)
    {
        printf("> ");
        fflush(stdout);

        // A. Sprawdzamy, czy przyszła wiadomość z wątku SUB (non-blocking recv)
        char received_chat_message_buffer[1024];
        int  received_message_bytes_number = zmq_recv(
            socket_pair_sub, received_chat_message_buffer, sizeof(received_chat_message_buffer) - 1, ZMQ_DONTWAIT);
        if (received_message_bytes_number > 0)
        {
            received_chat_message_buffer[received_message_bytes_number] = '\0';
            print_received_chat_message(received_chat_message_buffer);
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
            zmq_send(socket_pair_command, "/quit", 5, 0);
            break;
        }

        // Wysyłamy komendę do REQ i czekamy na Odpowiedź (ACK/ERR)
        zmq_send(socket_pair_command, line, strlen(line), 0);

        char response_buf[2048];
        int  bytes = zmq_recv(socket_pair_command, response_buf, sizeof(response_buf) - 1, 0);
        if (bytes > 0)
        {
            response_buf[bytes] = '\0';
            printf("%s\n", response_buf);
        }
    }

    zmq_close(socket_pair_command);
    zmq_close(socket_pair_sub);
    zmq_close(socket_sub_shutdown);
    printf("[ZMQClient][UI Thread] exit\n");
    return NULL;
}

static void clean_up(void* socket_sub_shutdown, void* socket_pair_command, void* socket_pair_ui)

{
    if (socket_sub_shutdown)
    {
        zmq_close(socket_sub_shutdown);
    }
    if (socket_pair_command)
    {
        zmq_close(socket_pair_command);
    }
    if (socket_pair_ui)
    {
        zmq_close(socket_pair_ui);
    }
}

static void print_received_chat_message(char* chat_message)
{
    printf("\r%s\n> ", chat_message);  // \r czyści bieżący znak zachowując kursor
}
