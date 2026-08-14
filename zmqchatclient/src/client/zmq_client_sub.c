#include "zmq_client_sub.h"
#include "zmq_client_common.h"

#include "generated/chat.pb-c.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <zmq.h>

static void handle_pub_sub_message(void* subscriber);
static void handle_command(void* ctrl_sub, void* subscriber);

void* subscriber_routine(void* arg)
{
    printf("[ZMQClient][SUB Thread] start subscriber_routine\n");
    SubscriberArgs* args    = (SubscriberArgs*)arg;
    void*           context = args->context;

    volatile sig_atomic_t* running    = args->running;
    void*                  subscriber = zmq_socket(context, ZMQ_SUB);
    if (zmq_connect(subscriber, SUB_ADDRESS) != 0)
    {
        perror("[ZMQClient][SUB Thread] Błąd połączenia z PUB");
        free(args);
        return NULL;
    }  // 1. general room sub

    void* ctrl_sub = zmq_socket(context, ZMQ_PAIR);
    if (zmq_connect(ctrl_sub, SUB_CONTROLL_ADDRESS) != 0)
    {
        perror("[SUB Thread] Błąd połączenia z  SUB_CONTROLL_ADDRESS");
        zmq_close(subscriber);
        free(args);
        return NULL;
    }

    char user_topic[128], room_general[128];
    snprintf(user_topic, sizeof(user_topic), "user:%s", args->username);
    snprintf(room_general, sizeof(room_general), "room:general");

    zmq_setsockopt(subscriber, ZMQ_SUBSCRIBE, user_topic, strlen(user_topic));
    zmq_setsockopt(subscriber, ZMQ_SUBSCRIBE, room_general, strlen(room_general));

    zmq_pollitem_t items[] = {
        { subscriber, 0, ZMQ_POLLIN, 0 },  // 0: Wiadomości sieciowe (PUB/SUB)
        { ctrl_sub, 0, ZMQ_POLLIN, 0 }     // 1: Komendy sterujące z głównego wątku (inproc)
    };

    while (running && *running)
    {
        // Czekamy na zdarzenia na dowolnym z 2 gniazd
        int rc = zmq_poll(items, 2, -1);
        if (rc < 0)
        {
            break;
        }
        // --- OBSŁUGA ZDARZEŃ SIECIOWYCH (PUB/SUB) ---
        if (items[0].revents & ZMQ_POLLIN)
        {
            handle_pub_sub_message(subscriber);
        }
        // --- OBSŁUGA KOMEND Z GŁÓWNEGO WĄTKU (INPROC) ---
        if (items[1].revents & ZMQ_POLLIN)
        {
            handle_command(ctrl_sub, subscriber);
        }
    }
    zmq_close(subscriber);
    zmq_close(ctrl_sub);
    free(args);
    printf("[ZMQClient][SUB Thread] exit\n");
    return NULL;
}

static void handle_pub_sub_message(void* subscriber)
{
    printf("[ZMQClient][SUB Thread] handle_pub_sub_message\n");
    char topic_buffer[256];
    int  topic_len = zmq_recv(subscriber, topic_buffer, sizeof(topic_buffer) - 1, 0);
    if (topic_len > 0)
    {
        topic_buffer[topic_len] = '\0';

        uint8_t data_buffer[2048];
        int     data_len = zmq_recv(subscriber, data_buffer, sizeof(data_buffer), 0);
        if (data_len > 0)
        {
            Api__Chat__MessageEnvelope* message_envelope
                = api__chat__message_envelope__unpack(NULL, data_len, data_buffer);
            if (message_envelope != NULL)
            {
                switch (message_envelope->payload_case)
                {
                case API__CHAT__MESSAGE_ENVELOPE__PAYLOAD_ROOM_MSG:
                {
                    Api__Chat__RoomMessage* room_message = message_envelope->room_msg;
                    printf("\n[ROOM #%s] %s: %s\n> ",
                           room_message->room_name,
                           room_message->sender_username,
                           room_message->content);
                    fflush(stdout);
                    break;
                }
                case API__CHAT__MESSAGE_ENVELOPE__PAYLOAD_DIRECT_MSG:
                {
                    Api__Chat__DirectMessage* direct_message = message_envelope->direct_msg;
                    printf("\n[DM from %s]: %s\n> ", direct_message->sender_username, direct_message->content);
                    fflush(stdout);
                    break;
                }
                case API__CHAT__MESSAGE_ENVELOPE__PAYLOAD_SYSTEM_NOTIF:
                {
                    Api__Chat__SystemNotification* notification_message = message_envelope->system_notif;
                    printf("\n[SYSTEM #%s]: %s\n> ", notification_message->room_name, notification_message->message);
                    fflush(stdout);
                    break;
                }
                default:
                    break;
                }
                api__chat__message_envelope__free_unpacked(message_envelope, NULL);
            }
        }
    }
}

static void handle_command(void* ctrl_sub, void* subscriber)
{
    printf("[ZMQClient][SUB Thread] handle_command\n");
    char ctrl_buf[256];
    int  len = zmq_recv(ctrl_sub, ctrl_buf, sizeof(ctrl_buf) - 1, 0);
    if (len > 0)
    {
        ctrl_buf[len] = '\0';

        // Format komendy: "+topic" (subskrybuj) lub "-topic" (odsubskrybuj)
        if (ctrl_buf[0] == '+')
        {
            char* topic = ctrl_buf + 1;
            zmq_setsockopt(subscriber, ZMQ_SUBSCRIBE, topic, strlen(topic));
        }
        else if (ctrl_buf[0] == '-')
        {
            char* topic = ctrl_buf + 1;
            zmq_setsockopt(subscriber, ZMQ_UNSUBSCRIBE, topic, strlen(topic));
        }
    }
}
