#include "zmq_client_req.h"
#include "zmq_client_common.h"

#include "generated/chat.pb-c.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <zmq.h>
#include <poll.h>

// Helper wysyłający i odbierający na REQ
static Api__Chat__MessageEnvelope* send_and_recv_env(void* requester, Api__Chat__MessageEnvelope* envelope);

static void handle_rooms_request(void* requester, void* command_socket);

static void handle_room_join_request(void* requester, void* ctrl_pub, void* command_socket, char* line, char* username);

static void handle_room_leave_request(
    void* requester, void* ctrl_pub, void* command_socket, char* line, char* username);

static void handle_message_room_request(void* requester, void* command_socket, char* line, char* username);

static void handle_message_direct_request(void* requester, void* command_socket, char* line, char* username);

static void clean_up(void* command_socket, void* requester, void* ctrl_pub, RequesterArgs* args);

void* requester_routine(void* arg)
{
    printf("[ZMQClient][REQ Thread] start requester_routine\n");

    struct pollfd pfd;
    pfd.fd     = STDIN_FILENO;
    pfd.events = POLLIN;

    RequesterArgs*         args     = (RequesterArgs*)arg;
    void*                  context  = args->context;
    char*                  username = args->username;
    volatile sig_atomic_t* running  = args->running;

    // 1. Gniazdo SIECIOWE (REQ)
    void* requester = zmq_socket(context, ZMQ_REQ);
    if (zmq_connect(requester, REQ_ADDRESS) != 0)
    {
        perror("[REQ Thread] REQ connection error");
        clean_up(NULL, requester, NULL, args);
        return NULL;
    }

    // 2. Gniazdo STERUJĄCE (PAIR do wątku SUB)
    void* ctrl_pub = zmq_socket(context, ZMQ_PAIR);
    if (zmq_bind(ctrl_pub, SUB_CONTROLL_ADDRESS) != 0)
    {
        perror("[REQ Thread] bind inproc://sub-control error");
        clean_up(NULL, requester, ctrl_pub, args);
        return NULL;
    }

    // 3. Gniazdo KOMEND (PAIR z wątkiem UI)
    void* command_socket = zmq_socket(context, ZMQ_PAIR);
    if (zmq_bind(command_socket, COMMAND_SOCKET_ADDRESS) != 0)
    {
        perror("[REQ Thread] bind inproc://command-socket error");
        clean_up(NULL, requester, ctrl_pub, args);
        return NULL;
    }

    // 4. Logowanie automatyczne przy starcie
    Api__Chat__LoginRequest login_req = API__CHAT__LOGIN_REQUEST__INIT;
    login_req.username                = username;
    login_req.client_version          = "1.0.0";

    Api__Chat__MessageEnvelope env = API__CHAT__MESSAGE_ENVELOPE__INIT;
    env.message_id                 = "req-login";
    env.payload_case               = API__CHAT__MESSAGE_ENVELOPE__PAYLOAD_LOGIN_REQ;
    env.login_req                  = &login_req;

    Api__Chat__MessageEnvelope* resp = send_and_recv_env(requester, &env);
    if (!resp || resp->payload_case != API__CHAT__MESSAGE_ENVELOPE__PAYLOAD_LOGIN_RESP
        || resp->login_resp->status != API__CHAT__STATUS__STATUS_OK)
    {
        printf("[REQ Thread] login failed!\n");
        clean_up(command_socket, requester, ctrl_pub, args);
        return NULL;
    }
    api__chat__message_envelope__free_unpacked(resp, NULL);
    printf("[REQ Thread] login success as '%s'!\n", username);

    // 5. Pętla obsługi poleceń z wątku UI
    while (running && *running)
    {
        char cmd_buf[512];
        int  bytes = zmq_recv(command_socket, cmd_buf, sizeof(cmd_buf) - 1, 0);
        if (bytes <= 0)
        {
            break;
        }
        cmd_buf[bytes] = '\0';

        if (strcmp(cmd_buf, "/quit") == 0)
        {
            break;
        }

        if (strcmp(cmd_buf, "/rooms") == 0)
        {
            handle_rooms_request(requester, command_socket);
        }
        else if (strncmp(cmd_buf, "/join ", 6) == 0)
        {
            handle_room_join_request(requester, ctrl_pub, command_socket, cmd_buf, username);
        }
        else if (strncmp(cmd_buf, "/leave ", 7) == 0)
        {
            handle_room_leave_request(requester, ctrl_pub, command_socket, cmd_buf, username);
        }
        else if (strncmp(cmd_buf, "/msg ", 5) == 0)
        {
            handle_message_room_request(requester, command_socket, cmd_buf, username);
        }
        else if (strncmp(cmd_buf, "/dm ", 4) == 0)
        {
            handle_message_direct_request(requester, command_socket, cmd_buf, username);
        }
        else
        {
            zmq_send(command_socket, "Nieznana komenda.", 17, 0);
        }
    }

    clean_up(command_socket, requester, ctrl_pub, args);
    printf("[ZMQClient][REQ Thread] exit\n");
    return NULL;
}

static void handle_rooms_request(void* requester, void* command_socket)
{
    Api__Chat__ListRoomsRequest list_req = API__CHAT__LIST_ROOMS_REQUEST__INIT;
    Api__Chat__MessageEnvelope  req_env  = API__CHAT__MESSAGE_ENVELOPE__INIT;
    req_env.message_id                   = "cmd-rooms";
    req_env.payload_case                 = API__CHAT__MESSAGE_ENVELOPE__PAYLOAD_LIST_ROOMS_REQ;
    req_env.list_rooms_req               = &list_req;

    Api__Chat__MessageEnvelope* res_env = send_and_recv_env(requester, &req_env);

    char out_buf[1024] = "Nie udało się pobrać listy pokojów.";
    if (res_env && res_env->payload_case == API__CHAT__MESSAGE_ENVELOPE__PAYLOAD_LIST_ROOMS_RESP)
    {
        int offset = snprintf(
            out_buf, sizeof(out_buf), "--- Lista Pokojów (%size_t) ---\n", res_env->list_rooms_resp->n_rooms);
        for (size_t i = 0; i < res_env->list_rooms_resp->n_rooms; i++)
        {
            Api__Chat__RoomInfo* r = res_env->list_rooms_resp->rooms[i];
            offset += snprintf(out_buf + offset,
                               sizeof(out_buf) - offset,
                               " * Pokój: #%-10s | Członków: %d\n",
                               r->name,
                               r->member_count);
        }
    }

    zmq_send(command_socket, out_buf, strlen(out_buf), 0);

    if (res_env)
    {
        api__chat__message_envelope__free_unpacked(res_env, NULL);
    }
}

static void handle_room_join_request(void* requester, void* ctrl_pub, void* command_socket, char* line, char* username)
{
    char* room_name = line + 6;

    Api__Chat__JoinRoomRequest join_req = API__CHAT__JOIN_ROOM_REQUEST__INIT;
    join_req.room_name                  = room_name;
    join_req.username                   = username;

    Api__Chat__MessageEnvelope req_env = API__CHAT__MESSAGE_ENVELOPE__INIT;
    req_env.message_id                 = "cmd-join";
    req_env.payload_case               = API__CHAT__MESSAGE_ENVELOPE__PAYLOAD_JOIN_ROOM_REQ;
    req_env.join_room_req              = &join_req;

    Api__Chat__MessageEnvelope* res_env = send_and_recv_env(requester, &req_env);

    char out_buf[256] = "Błąd dołączania do pokoju.";
    if (res_env && res_env->payload_case == API__CHAT__MESSAGE_ENVELOPE__PAYLOAD_ACK)
    {
        snprintf(out_buf, sizeof(out_buf), "[SERWER]: %s", res_env->ack->message);

        // Wysyłamy komendę do wątku SUB po szynie inproc
        char ctrl_cmd[128];
        snprintf(ctrl_cmd, sizeof(ctrl_cmd), "+room:%s", room_name);
        zmq_send(ctrl_pub, ctrl_cmd, strlen(ctrl_cmd), 0);
    }

    zmq_send(command_socket, out_buf, strlen(out_buf), 0);

    if (res_env)
    {
        api__chat__message_envelope__free_unpacked(res_env, NULL);
    }
}

static Api__Chat__MessageEnvelope* send_and_recv_env(void* requester, Api__Chat__MessageEnvelope* envelope)
{
    size_t   envelope_packed_size = api__chat__message_envelope__get_packed_size(envelope);
    uint8_t* envelope_buffer      = malloc(envelope_packed_size);
    api__chat__message_envelope__pack(envelope, envelope_buffer);

    zmq_send(requester, envelope_buffer, envelope_packed_size, 0);
    free(envelope_buffer);

    uint8_t received_buffer[4096];
    int     received_bytes_number = zmq_recv(requester, received_buffer, sizeof(received_buffer), 0);
    if (received_bytes_number < 0)
        return NULL;

    return api__chat__message_envelope__unpack(NULL, received_bytes_number, received_buffer);
}

static void handle_room_leave_request(void* requester, void* ctrl_pub, void* command_socket, char* line, char* username)
{
    char* room_name = line + 7;

    Api__Chat__LeaveRoomRequest leave_req = API__CHAT__LEAVE_ROOM_REQUEST__INIT;
    leave_req.room_name                   = room_name;
    leave_req.username                    = username;

    Api__Chat__MessageEnvelope req_env = API__CHAT__MESSAGE_ENVELOPE__INIT;
    req_env.message_id                 = "cmd-leave";
    req_env.payload_case               = API__CHAT__MESSAGE_ENVELOPE__PAYLOAD_LEAVE_ROOM_REQ;
    req_env.leave_room_req             = &leave_req;

    Api__Chat__MessageEnvelope* res_env = send_and_recv_env(requester, &req_env);

    char out_buf[256] = "Błąd opuszczania pokoju.";
    if (res_env && res_env->payload_case == API__CHAT__MESSAGE_ENVELOPE__PAYLOAD_ACK)
    {
        snprintf(out_buf, sizeof(out_buf), "[SERWER]: %s", res_env->ack->message);

        char ctrl_cmd[128];
        snprintf(ctrl_cmd, sizeof(ctrl_cmd), "-room:%s", room_name);
        zmq_send(ctrl_pub, ctrl_cmd, strlen(ctrl_cmd), 0);
    }

    zmq_send(command_socket, out_buf, strlen(out_buf), 0);

    if (res_env)
    {
        api__chat__message_envelope__free_unpacked(res_env, NULL);
    }
}

static void handle_message_room_request(void* requester, void* command_socket, char* line, char* username)
{
    char* room_name = strtok(line + 5, " ");
    char* content   = strtok(NULL, "");

    if (room_name && content)
    {
        Api__Chat__RoomMessage rmsg = API__CHAT__ROOM_MESSAGE__INIT;
        rmsg.room_name              = room_name;
        rmsg.sender_username        = username;
        rmsg.content                = content;

        Api__Chat__MessageEnvelope req_env = API__CHAT__MESSAGE_ENVELOPE__INIT;
        req_env.message_id                 = "cmd-msg";
        req_env.payload_case               = API__CHAT__MESSAGE_ENVELOPE__PAYLOAD_ROOM_MSG;
        req_env.room_msg                   = &rmsg;

        Api__Chat__MessageEnvelope* res_env = send_and_recv_env(requester, &req_env);
        if (res_env)
        {
            zmq_send(command_socket, "[ACK] Wiadomość wysłana", 22, 0);
            api__chat__message_envelope__free_unpacked(res_env, NULL);
        }
        else
        {
            zmq_send(command_socket, "[ERR] Błąd wysyłania wiadomości", 31, 0);
        }
    }
    else
    {
        zmq_send(command_socket, "Użycie: /msg <nazwa_pokoju> <treść>", 35, 0);
    }
}

static void handle_message_direct_request(void* requester, void* command_socket, char* line, char* username)
{
    char* target  = strtok(line + 4, " ");
    char* content = strtok(NULL, "");

    if (target && content)
    {
        Api__Chat__DirectMessage dm = API__CHAT__DIRECT_MESSAGE__INIT;
        dm.recipient_username       = target;
        dm.sender_username          = username;
        dm.content                  = content;

        Api__Chat__MessageEnvelope req_env = API__CHAT__MESSAGE_ENVELOPE__INIT;
        req_env.message_id                 = "cmd-dm";
        req_env.payload_case               = API__CHAT__MESSAGE_ENVELOPE__PAYLOAD_DIRECT_MSG;
        req_env.direct_msg                 = &dm;

        Api__Chat__MessageEnvelope* res_env = send_and_recv_env(requester, &req_env);
        if (res_env)
        {
            zmq_send(command_socket, "[ACK] Wiadomość prywatna wysłana", 32, 0);
            api__chat__message_envelope__free_unpacked(res_env, NULL);
        }
        else
        {
            zmq_send(command_socket, "[ERR] Błąd wysyłania DM", 23, 0);
        }
    }
    else
    {
        zmq_send(command_socket, "Użycie: /dm <użytkownik> <treść>", 32, 0);
    }
}

static void clean_up(void* command_socket, void* requester, void* ctrl_pub, RequesterArgs* args)
{
    if (command_socket)
    {
        zmq_close(command_socket);
    }
    if (requester)
    {
        zmq_close(requester);
    }
    if (ctrl_pub)
    {
        zmq_close(ctrl_pub);
    }
    if (args)
    {
        free(args);
    }
}
