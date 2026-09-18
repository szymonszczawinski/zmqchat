#include "chat_client_receiver.h"
#include "chat_client_common.h"
#include "utils/chat_client_socket.h"
#include "generated/chat.pb-c.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <zmq.h>

#define OK_ZMQ 0
static void handle_chat_message(socket_sub_t* socket_sub_chat, socket_pair_t* socket_pair_ui_notification);
static void handle_command(socket_pair_t* socket_pair_controller_command,
                           socket_sub_t*  socket_sub_chat,
                           socket_push_t* socket_push_logs);

static void clean_up(socket_sub_t*  socket_sub_shutdown,
                     socket_sub_t*  socket_sub_chat,
                     socket_pair_t* socket_pair_controller_command,
                     socket_pair_t* socket_pair_ui_notification);

static void log_app(socket_push_t* socket_push_logs, const char* format, ...);

static int handle_shutdown_message(socket_sub_t* socket_sub_shutdown, socket_push_t* socket_push_logs);

void* routine_chat_receiver(void* arg)
{
    ReceiverArgs* args    = (ReceiverArgs*)arg;
    void*         context = args->context;

    socket_push_t* socket_push_logs = socket_push_new(context);
    if (socket_push_connect(socket_push_logs, APP_LOGS_ADDRESS) != 0)
    {
        perror("[ZMQClient][controller thread] connect inproc://app_log error");
        socket_push_destroy(&socket_push_logs);
        return NULL;
    }
    log_app(socket_push_logs, "[ZMQClient][receiver thread] start subscriber_routine\n");
    // Control socket INPROC (SUB for kill/stop signal)
    socket_sub_t* socket_sub_shutdown = socket_sub_new(context);
    if (socket_sub_connect(socket_sub_shutdown, INPROC_SHUTDOWN_ADDR) == -1)
    {
        log_app(socket_push_logs, "[ZMQClient][receiver thread] shutdown socket connect failed: %s\n", strerror(errno));
        clean_up(socket_sub_shutdown, NULL, NULL, NULL);
        return NULL;
    }
    if (socket_sub_subscribe(socket_sub_shutdown, "") != 0)
    {
        log_app(socket_push_logs, "[ZMQClient][UI Thread] shutdown socket subscribe failed: %s\n", strerror(errno));
        clean_up(socket_sub_shutdown, NULL, NULL, NULL);
        return NULL;
    }

    // SUB socket for subscribing for server chat messages
    socket_sub_t* socket_sub_chat = socket_sub_new(context);
    if (socket_sub_connect(socket_sub_chat, CHAT_SERVER_SUB_ADDRESS) != 0)
    {
        log_app(socket_push_logs, "[ZMQClient][receiver thread] Błąd połączenia z PUB");
        clean_up(socket_sub_shutdown, socket_sub_chat, NULL, NULL);
        return NULL;
    }

    // PAIR controller commands socket
    socket_pair_t* socket_pair_controller_command = socket_pair_new(context);
    if (socket_pair_connect(socket_pair_controller_command, CONTROLLER_RECEIVER_COMMAND_ADDRESS) != 0)
    {
        log_app(socket_push_logs, "[receiver thread] Błąd połączenia z CONTROLLER_RECEIVER_COMMAND_ADDRESS");
        clean_up(socket_sub_shutdown, socket_sub_chat, socket_pair_controller_command, NULL);
        return NULL;
    }

    // PAIR ui notifications socket
    socket_pair_t* socket_pair_ui_notification = socket_pair_new(context);
    if (socket_pair_connect(socket_pair_ui_notification, CONTROLLER_UI_NOTIFICATION_ADDRESS) != 0)
    {
        log_app(socket_push_logs, "[receiver thread] Błąd połączenia z UI_NOTIF_ADDRESS");
        clean_up(socket_sub_shutdown, socket_sub_chat, socket_pair_controller_command, socket_pair_ui_notification);
        return NULL;
    }
    // Subskrypcje początkowe
    char user_topic[128], room_general[128];
    snprintf(user_topic, sizeof(user_topic), CHAT_TOPIC_USER, args->username);
    snprintf(room_general, sizeof(room_general), CHAT_TOPIC_GENERAL);

    socket_sub_subscribe(socket_sub_chat, user_topic);
    socket_sub_subscribe(socket_sub_chat, room_general);

    zmq_pollitem_t items[] = {
        { socket_sub_get_raw(socket_sub_shutdown), 0, ZMQ_POLLIN, 0 },  // 0: Sygnał wyłączenia
        { socket_sub_get_raw(socket_sub_chat), 0, ZMQ_POLLIN, 0 },      // 1: Wiadomości sieciowe (PUB/SUB)
        { socket_pair_get_raw(socket_pair_controller_command), 0, ZMQ_POLLIN, 0 }
        // 2: Komendy sterujące z REQ (inproc)
    };

    log_app(socket_push_logs, "[ZMQClient][receiver thread] loop start!\n");
    while (1)
    {
        log_app(socket_push_logs, "[ZMQClient][receiver thread] waiting...\n");
        int rc = zmq_poll(items, 3, -1);
        if (rc < 0)
        {
            break;
        }
        if (items[0].revents & ZMQ_POLLIN)
        {
            if (handle_shutdown_message(socket_sub_shutdown, socket_push_logs) < 0)
            {
                break;
            }
        }
        if (items[1].revents & ZMQ_POLLIN)
        {
            handle_chat_message(socket_sub_chat, socket_pair_ui_notification);
        }

        if (items[2].revents & ZMQ_POLLIN)
        {
            handle_command(socket_pair_controller_command, socket_sub_chat, socket_push_logs);
        }
    }
    clean_up(socket_sub_shutdown, socket_sub_chat, socket_pair_controller_command, socket_pair_ui_notification);
    log_app(socket_push_logs, "[ZMQClient][receiver thread] exit\n");
    socket_push_destroy(&socket_push_logs);
    return NULL;
}

static void handle_chat_message(socket_sub_t* socket_sub_chat, socket_pair_t* socket_pair_ui_notification)
{
    char topic_buffer[256];
    int  topic_len = socket_sub_recv(socket_sub_chat, topic_buffer, sizeof(topic_buffer) - 1, 0);
    if (topic_len <= 0)
        return;

    topic_buffer[topic_len] = '\0';

    uint8_t data_buffer[2048];
    int     data_len = socket_sub_recv(socket_sub_chat, data_buffer, sizeof(data_buffer), 0);
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
        socket_pair_send(socket_pair_ui_notification, formatted_msg, strlen(formatted_msg), 0);
    }

    api__chat__message_envelope__free_unpacked(envelope, NULL);
}

static void handle_command(socket_pair_t* socket_pair_controller_command,
                           socket_sub_t*  socket_sub_chat,
                           socket_push_t* socket_push_logs)
{
    log_app(socket_push_logs, "[ZMQClient][receiver thread] handle_command\n");
    char ctrl_buf[256];
    int  len = socket_pair_recv(socket_pair_controller_command, ctrl_buf, sizeof(ctrl_buf) - 1, 0);
    if (len > 0)
    {
        ctrl_buf[len] = '\0';

        // Format komendy: "+topic" (subskrybuj) lub "-topic" (odsubskrybuj)
        if (ctrl_buf[0] == '+')
        {
            char* topic = ctrl_buf + 1;
            socket_sub_subscribe(socket_sub_chat, topic);
        }
        else if (ctrl_buf[0] == '-')
        {
            char* topic = ctrl_buf + 1;
            socket_sub_unsubscribe(socket_sub_chat, topic);
        }
    }
}

static void clean_up(socket_sub_t*  socket_sub_shutdown,
                     socket_sub_t*  socket_sub_chat,
                     socket_pair_t* socket_pair_controller_command,
                     socket_pair_t* socket_pair_ui_notification)
{
    socket_sub_destroy(&socket_sub_shutdown);
    socket_sub_destroy(&socket_sub_chat);
    socket_pair_destroy(&socket_pair_controller_command);
    socket_pair_destroy(&socket_pair_ui_notification);
}

static void log_app(socket_push_t* socket_push_logs, const char* format, ...)
{
    char    body[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(body, sizeof(body), format, args);
    va_end(args);

    char full_log[1100];
    snprintf(full_log, sizeof(full_log), "%s", body);

    // Wysyłamy log przez PUB (nieblokująco)
    socket_push_send(socket_push_logs, full_log, strlen(full_log), ZMQ_DONTWAIT);
}

static int handle_shutdown_message(socket_sub_t* socket_sub_shutdown, socket_push_t* socket_push_logs)
{
    char shutdown_msg[64] = { 0 };
    int  bytes            = socket_sub_recv(socket_sub_shutdown, shutdown_msg, sizeof(shutdown_msg) - 1, 0);
    if (bytes > 0)
    {
        if (bytes >= (int)sizeof(shutdown_msg))
        {
            bytes = sizeof(shutdown_msg) - 1;
        }
        shutdown_msg[bytes] = '\0';
        if (strcmp(shutdown_msg, CHAT_MESSAGE_KILL_CONTROLLER) == 0)
        {
            log_app(socket_push_logs, "[ZMQClient][receiver thread] received KILL signal. shutting down cleanly...\n");
            return -1;
        }
    }
    return OK_ZMQ;
}
