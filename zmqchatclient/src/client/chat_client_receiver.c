#include "chat_client_receiver.h"
#include "chat_client_common.h"

#include "generated/chat.pb-c.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <zmq.h>

static void handle_chat_message(void* subscriber, void* ui_pub);
static void handle_command(void* ctrl_sub, void* subscriber);

void* routine_chat_receiver(void* arg)
{
    printf("[ZMQClient][SUB Thread] start subscriber_routine\n");
    ReceiverArgs* args    = (ReceiverArgs*)arg;
    void*         context = args->context;

    // Control socket INPROC (SUB for kill/stop signal)
    void* socket_sub_shutdown = zmq_socket(context, ZMQ_SUB);
    if (zmq_connect(socket_sub_shutdown, INPROC_SHUTDOWN_ADDR) == -1)
    {
        printf("[ZMQClient][SUB Thread] shutdown socket connect failed: %s\n", strerror(errno));
        return NULL;
    }
    if (zmq_setsockopt(socket_sub_shutdown, ZMQ_SUBSCRIBE, "", 0) != 0)
    {
        printf("[ZMQClient][UI Thread] shutdown socket subscribe failed: %s\n", strerror(errno));
        zmq_close(socket_sub_shutdown);
        return NULL;
    }

    // SUB socket for subscribing for server chat messages
    void* socket_sub_chat = zmq_socket(context, ZMQ_SUB);
    if (zmq_connect(socket_sub_chat, CHAT_SERVER_SUB_ADDRESS) != 0)
    {
        perror("[ZMQClient][SUB Thread] Błąd połączenia z PUB");
        return NULL;
    }

    // PAIR controller commands socket
    void* socket_pair_controller_command = zmq_socket(context, ZMQ_PAIR);
    if (zmq_connect(socket_pair_controller_command, CONTROLLER_RECEIVER_COMMAND_ADDRESS) != 0)
    {
        perror("[SUB Thread] Błąd połączenia z CONTROLLER_RECEIVER_COMMAND_ADDRESS");
        zmq_close(socket_sub_chat);
        return NULL;
    }

    // PAIR ui notifications socket
    void* socket_pair_ui_notification = zmq_socket(context, ZMQ_PAIR);
    if (zmq_connect(socket_pair_ui_notification, CONTROLLER_UI_NOTIFICATION_ADDRESS) != 0)
    {
        perror("[SUB Thread] Błąd połączenia z UI_NOTIF_ADDRESS");
        zmq_close(socket_sub_chat);
        zmq_close(socket_pair_controller_command);
        return NULL;
    }
    // Subskrypcje początkowe
    char user_topic[128], room_general[128];
    snprintf(user_topic, sizeof(user_topic), "user:%s", args->username);
    snprintf(room_general, sizeof(room_general), "room:general");

    zmq_setsockopt(socket_sub_chat, ZMQ_SUBSCRIBE, user_topic, strlen(user_topic));
    zmq_setsockopt(socket_sub_chat, ZMQ_SUBSCRIBE, room_general, strlen(room_general));

    zmq_pollitem_t items[] = {
        { socket_sub_shutdown, 0, ZMQ_POLLIN, 0 },            // 0: Sygnał wyłączenia
        { socket_sub_chat, 0, ZMQ_POLLIN, 0 },                // 1: Wiadomości sieciowe (PUB/SUB)
        { socket_pair_controller_command, 0, ZMQ_POLLIN, 0 }  // 2: Komendy sterujące z REQ (inproc)
    };

    while (1)
    {
        int rc = zmq_poll(items, 3, -1);
        if (rc < 0)
        {
            break;
        }
        if (items[0].revents & ZMQ_POLLIN)
        {
            char shutdown_msg[1];  // Poprawna tablica znaków
            int  bytes = zmq_recv(socket_sub_shutdown, shutdown_msg, sizeof(shutdown_msg) - 1, 0);
            if (bytes > 0)
            {
                shutdown_msg[bytes] = '\0';
                if (strcmp(shutdown_msg, "KILL") == 0)
                {
                    printf("[ZMQClient][SUB Thread] Received KILL signal. Shutting down cleanly...\n");
                    break;
                }
            }
        }
        if (items[1].revents & ZMQ_POLLIN)
        {
            handle_chat_message(socket_sub_chat, socket_pair_ui_notification);
        }

        if (items[2].revents & ZMQ_POLLIN)
        {
            handle_command(socket_pair_controller_command, socket_sub_chat);
        }
    }
    zmq_close(socket_sub_chat);
    zmq_close(socket_pair_controller_command);
    zmq_close(socket_pair_ui_notification);
    zmq_close(socket_sub_shutdown);
    printf("[ZMQClient][SUB Thread] exit\n");
    return NULL;
}

static void handle_chat_message(void* socket_sub_chat, void* socket_pair_ui_notification)
{
    char topic_buffer[256];
    int  topic_len = zmq_recv(socket_sub_chat, topic_buffer, sizeof(topic_buffer) - 1, 0);
    if (topic_len <= 0)
        return;

    topic_buffer[topic_len] = '\0';

    uint8_t data_buffer[2048];
    int     data_len = zmq_recv(socket_sub_chat, data_buffer, sizeof(data_buffer), 0);
    if (data_len <= 0)
        return;

    Api__Chat__MessageEnvelope* envelope = api__chat__message_envelope__unpack(NULL, data_len, data_buffer);
    if (!envelope)
        return;

    char formatted_msg[1024];
    formatted_msg[0] = '\0';

    switch (envelope->payload_case)
    {
    case API__CHAT__MESSAGE_ENVELOPE__PAYLOAD_ROOM_MSG:
    {
        Api__Chat__RoomMessage* msg = envelope->room_msg;
        snprintf(formatted_msg,
                 sizeof(formatted_msg),
                 "[ROOM #%s] %s: %s",
                 msg->room_name,
                 msg->sender_username,
                 msg->content);
        break;
    }
    case API__CHAT__MESSAGE_ENVELOPE__PAYLOAD_DIRECT_MSG:
    {
        Api__Chat__DirectMessage* dm = envelope->direct_msg;
        snprintf(formatted_msg, sizeof(formatted_msg), "[DM from %s]: %s", dm->sender_username, dm->content);
        break;
    }
    case API__CHAT__MESSAGE_ENVELOPE__PAYLOAD_SYSTEM_NOTIF:
    {
        Api__Chat__SystemNotification* sys = envelope->system_notif;
        snprintf(formatted_msg, sizeof(formatted_msg), "[SYSTEM #%s]: %s", sys->room_name, sys->message);
        break;
    }
    default:
        break;
    }

    // Wysyłamy sformatowaną wiadomość tekstową do wątku UI
    if (strlen(formatted_msg) > 0)
    {
        zmq_send(socket_pair_ui_notification, formatted_msg, strlen(formatted_msg), 0);
    }

    api__chat__message_envelope__free_unpacked(envelope, NULL);
}

static void handle_command(void* socket_pair_controller_command, void* socket_sub_chat)
{
    printf("[ZMQClient][SUB Thread] handle_command\n");
    char ctrl_buf[256];
    int  len = zmq_recv(socket_pair_controller_command, ctrl_buf, sizeof(ctrl_buf) - 1, 0);
    if (len > 0)
    {
        ctrl_buf[len] = '\0';

        // Format komendy: "+topic" (subskrybuj) lub "-topic" (odsubskrybuj)
        if (ctrl_buf[0] == '+')
        {
            char* topic = ctrl_buf + 1;
            zmq_setsockopt(socket_sub_chat, ZMQ_SUBSCRIBE, topic, strlen(topic));
        }
        else if (ctrl_buf[0] == '-')
        {
            char* topic = ctrl_buf + 1;
            zmq_setsockopt(socket_sub_chat, ZMQ_UNSUBSCRIBE, topic, strlen(topic));
        }
    }
}
