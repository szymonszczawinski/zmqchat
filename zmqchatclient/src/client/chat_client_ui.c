#include "chat_client_ui.h"
#include "chat_client_common.h"
#include "chat_client_socket.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <zmq.h>
#include <poll.h>

static void clean_up(socket_sub_t*  socket_sub_shutdown,
                     socket_pair_t* socket_pair_command,
                     socket_pair_t* socket_pair_sub);
static void print_received_chat_message(char* chat_message);

void* routine_chat_ui(void* arg)
{
    printf("[ZMQClient][UI Thread] start ui_routine\n");

    UiArgs* args    = (UiArgs*)arg;
    void*   context = args->context;

    // Control socket INPROC (SUB for kill/stop signal)
    socket_sub_t* socket_sub_shutdown = socket_sub_new(context);
    if (socket_sub_connect(socket_sub_shutdown, INPROC_SHUTDOWN_ADDR) == -1)
    {
        printf("[ZMQClient][UI Thread] shutdown socket connect failed: %s\n", strerror(errno));
        socket_sub_destroy(&socket_sub_shutdown);
        return NULL;
    }
    if (socket_sub_subscribe(socket_sub_shutdown, "") != 0)
    {
        printf("[ZMQClient][UI Thread] shutdown socket subscribe failed: %s\n", strerror(errno));
        socket_sub_destroy(&socket_sub_shutdown);
        return NULL;
    }

    // 1. Gniazdo do wysyłania komend do wątku REQ
    socket_pair_t* socket_pair_command = socket_pair_new(context);
    if (socket_pair_connect(socket_pair_command, CONROLLER_UI_COMMAND_ADDRESS) != 0)
    {
        perror("[ZMQClient][UI Thread] connect inproc://command error");
        clean_up(socket_sub_shutdown, socket_pair_command, NULL);
        return NULL;
    }

    // 2. Gniazdo do odbierania wiadomości z wątku SUB
    socket_pair_t* socket_pair_sub = socket_pair_new(context);
    if (socket_pair_bind(socket_pair_sub, CONTROLLER_UI_NOTIFICATION_ADDRESS) != 0)
    {
        perror("[ZMQClient][UI Thread] bind inproc://ui-notifications error");
        clean_up(socket_sub_shutdown, socket_pair_command, socket_pair_sub);
        return NULL;
    }

    printf("\nDostępne komendy:\n");
    printf("/rooms                     - lista pokojów\n");
    printf("/join <nazwa_pokoju>       - dołącza do pokoju\n");
    printf("/leave <nazwa_pokoju>      - opuszcza pokój\n");
    printf("/msg <nazwa_pokoju> <treść>- wysyła wiadomość\n");
    printf("/dm <użytkownik> <treść>   - wiadomość prywatna\n");
    printf("/quit                      - wyjście\n\n");

    zmq_pollitem_t items[] = {
        { socket_sub_get_raw(socket_sub_shutdown), 0, ZMQ_POLLIN, 0 },
        { socket_pair_get_raw(socket_pair_sub), 0, ZMQ_POLLIN, 0 },
        { NULL, STDIN_FILENO, ZMQ_POLLIN, 0 }  // Monitorowanie deskryptora systemowego
    };

    char line[512];
    printf("> ");
    fflush(stdout);

    printf("[ZMQClient][UI Thread] loop start!\n");
    while (1)
    {
        int rc = zmq_poll(items, 3, 100);
        if (rc < 0)
        {
            if (errno == EINTR)
            {
                continue;  // Przerwanie sygnałem systemowym - ponawiamy poll
            }
            perror("[ZMQClient][UI Thread] zmq_poll error");
            break;
        }

        // "KILL" signal
        if (items[0].revents & ZMQ_POLLIN)
        {
            char shutdown_msg[10] = { 0 };
            int  bytes            = socket_sub_recv(socket_sub_shutdown, shutdown_msg, sizeof(shutdown_msg) - 1, 0);
            if (bytes > 0)
            {
                shutdown_msg[bytes] = '\0';
                if (strcmp(shutdown_msg, CHAT_MESSAGE_KILL) == 0)
                {
                    printf("\n[ZMQClient][UI Thread] Received KILL signal. Shutting down cleanly...\n");
                    break;
                }
            }
        }

        //  SUB ->  UI
        if (items[1].revents & ZMQ_POLLIN)
        {
            char received_chat_message_buffer[1024];
            int  received_message_bytes_number = socket_pair_recv(
                socket_pair_sub, received_chat_message_buffer, sizeof(received_chat_message_buffer) - 1, 0);
            if (received_message_bytes_number > 0)
            {
                received_chat_message_buffer[received_message_bytes_number] = '\0';

                print_received_chat_message(received_chat_message_buffer);
                fflush(stdout);
            }
        }

        // STDIN
        if (items[2].revents & ZMQ_POLLIN)
        {
            if (!fgets(line, sizeof(line), stdin))
            {
                break;  // end of stream (np. Ctrl+D)
            }

            line[strcspn(line, "\r\n")] = 0;
            if (strlen(line) == 0)
            {
                printf("> ");
                fflush(stdout);
                continue;
            }

            if (strcmp(line, "/quit") == 0)
            {
                socket_pair_send(socket_pair_command, "/quit", 5, 0);
                raise(SIGINT);
                break;
            }

            // send to REQ and wait for (ACK/ERR)
            socket_pair_send(socket_pair_command, line, strlen(line), 0);

            char response_buf[2048];
            int  bytes = socket_pair_recv(socket_pair_command, response_buf, sizeof(response_buf) - 1, 0);
            if (bytes > 0)
            {
                response_buf[bytes] = '\0';
                print_received_chat_message(response_buf);
            }

            printf("> ");
            fflush(stdout);
        }
    }
    clean_up(socket_sub_shutdown, socket_pair_command, socket_pair_sub);
    printf("[ZMQClient][UI Thread] exit\n");
    return NULL;
}

static void clean_up(socket_sub_t*  socket_sub_shutdown,
                     socket_pair_t* socket_pair_command,
                     socket_pair_t* socket_pair_sub)
{
    socket_sub_destroy(&socket_sub_shutdown);
    socket_pair_destroy(&socket_pair_command);
    socket_pair_destroy(&socket_pair_sub);
}

static void print_received_chat_message(char* chat_message)
{
    printf("\r%s\n> ", chat_message);  // \r czyści bieżący znak zachowując kursor
}
