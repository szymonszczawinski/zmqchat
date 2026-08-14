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

static void handle_rooms_request(void* requester);

static void handle_room_join_request(void* requester, void* ctrl_pub, char* line, char* username);

static void handle_room_leave_request(void* requester, void* ctrl_pub, char* line, char* username);

static void handle_message_room_request(void* requester, char* line, char* username);

static void handle_message_direct_request(void* requester, char* line, char* username);

static void clean_up(void* requester, void* ctrl_pub, RequesterArgs* args);

void* requester_routine(void* arg)
{
    printf("[ZMQClient][REQ Thread] start requester_routine\n");

    struct pollfd pfd;
    pfd.fd     = STDIN_FILENO;
    pfd.events = POLLIN;

    RequesterArgs*         args      = (RequesterArgs*)arg;
    void*                  context   = args->context;
    char*                  username  = args->username;
    volatile sig_atomic_t* running   = args->running;
    void*                  requester = zmq_socket(context, ZMQ_REQ);
    if (zmq_connect(requester, REQ_ADDRESS) != 0)
    {
        perror("REQ connection error");
        clean_up(requester, NULL, args);
        return NULL;
    }

    void* ctrl_pub = zmq_socket(context, ZMQ_PAIR);
    if (zmq_bind(ctrl_pub, SUB_CONTROLL_ADDRESS) != 0)
    {
        perror("bind inproc://sub-control error");
        clean_up(requester, ctrl_pub, args);

        return NULL;
    }
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
        printf("login failed!\n");
        clean_up(requester, ctrl_pub, args);
        return NULL;
    }
    api__chat__message_envelope__free_unpacked(resp, NULL);
    printf("login success as '%s'!\n", username);

    char line[512];
    fgets(line, sizeof(line), stdin);  // czyszczenie bufora po scanf

    printf("> -1\n");
    printf("> ");
    while (running && *running)
    {
        fflush(stdout);

        // Czekamy 100 ms na wpisanie czegokolwiek w konsoli
        int poll_res = poll(&pfd, 1, 100);

        if (poll_res < 0)
        {
            // Przerwanie sygnałem (EINTR) - wychodzimy
            break;
        }
        if (poll_res == 0)
        {
            // Timeout (brak danych) - wracamy do początku pętli i sprawdzamy *running!
            continue;
        }

        if (!fgets(line, sizeof(line), stdin))
        {
            printf("> 0\n");
            // Jeśli fgets przerwał z powodu Ctrl+C (EINTR) lub flagi running
            if (!*running)
            {
                printf("> 1\n");
                break;
            }
            // Zwykły błąd lub EOF (np. Ctrl+D)
            break;
        }

        printf("> 2\n");
        line[strcspn(line, "\r\n")] = 0;
        if (strlen(line) == 0)
        {
            continue;
        }

        if (strcmp(line, "/quit") == 0)
        {
            *running = false;
            break;
        }

        // --- /rooms ---
        if (strcmp(line, "/rooms") == 0)
        {
            handle_rooms_request(requester);
        }
        else if (strncmp(line, "/join ", 6) == 0)
        {
            handle_room_join_request(requester, ctrl_pub, line, username);
        }
        else if (strncmp(line, "/leave ", 7) == 0)
        {
            handle_room_leave_request(requester, ctrl_pub, line, username);

            // --- /msg <room> <content> ---
        }
        else if (strncmp(line, "/msg ", 5) == 0)
        {
            handle_message_room_request(requester, line, username);

            // --- /dm <target> <content> ---
        }
        else if (strncmp(line, "/dm ", 4) == 0)
        {
            handle_message_direct_request(requester, line, username);
        }
    }
    clean_up(requester, ctrl_pub, args);
    printf("[ZMQClient][REQ Thread] exit\n");
    return NULL;
}

static void handle_rooms_request(void* requester)
{
    Api__Chat__ListRoomsRequest list_req = API__CHAT__LIST_ROOMS_REQUEST__INIT;
    Api__Chat__MessageEnvelope  req_env  = API__CHAT__MESSAGE_ENVELOPE__INIT;
    req_env.message_id                   = "cmd-rooms";
    req_env.payload_case                 = API__CHAT__MESSAGE_ENVELOPE__PAYLOAD_LIST_ROOMS_REQ;
    req_env.list_rooms_req               = &list_req;

    Api__Chat__MessageEnvelope* res_env = send_and_recv_env(requester, &req_env);
    if (res_env && res_env->payload_case == API__CHAT__MESSAGE_ENVELOPE__PAYLOAD_LIST_ROOMS_RESP)
    {
        printf("--- Lista Pokojów (%size_t) ---\n", res_env->list_rooms_resp->n_rooms);
        for (size_t i = 0; i < res_env->list_rooms_resp->n_rooms; i++)
        {
            Api__Chat__RoomInfo* r = res_env->list_rooms_resp->rooms[i];
            printf(" * Pokój: #%-10s | Członków: %d\n", r->name, r->member_count);
        }
    }
    if (res_env)
    {
        api__chat__message_envelope__free_unpacked(res_env, NULL);
    }

    // --- /join <room> ---
}

static void handle_room_join_request(void* requester, void* ctrl_pub, char* line, char* username)
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
    if (res_env && res_env->payload_case == API__CHAT__MESSAGE_ENVELOPE__PAYLOAD_ACK)
    {
        printf("[SERWER]: %s\n", res_env->ack->message);

        // BEZPIECZNE: Wysyłamy komendę do wątku SUB po szynie inproc!
        char ctrl_cmd[128];
        snprintf(ctrl_cmd, sizeof(ctrl_cmd), "+room:%s", room_name);
        zmq_send(ctrl_pub, ctrl_cmd, strlen(ctrl_cmd), 0);
    }
    if (res_env)
    {
        api__chat__message_envelope__free_unpacked(res_env, NULL);
    }
    // --- /leave <room> ---
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

static void handle_room_leave_request(void* requester, void* ctrl_pub, char* line, char* username)
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
    if (res_env && res_env->payload_case == API__CHAT__MESSAGE_ENVELOPE__PAYLOAD_ACK)
    {
        printf("[SERWER]: %s\n", res_env->ack->message);

        // BEZPIECZNE: Wysyłamy komendę odsubskrybowania po szynie inproc!
        char ctrl_cmd[128];
        snprintf(ctrl_cmd, sizeof(ctrl_cmd), "-room:%s", room_name);
        zmq_send(ctrl_pub, ctrl_cmd, strlen(ctrl_cmd), 0);
    }
    if (res_env)
        api__chat__message_envelope__free_unpacked(res_env, NULL);
}

static void handle_message_room_request(void* requester, char* line, char* username)
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
            api__chat__message_envelope__free_unpacked(res_env, NULL);
    }
    else
    {
        printf("Użycie: /msg <nazwa_pokoju> <treść>\n");
    }
}

static void handle_message_direct_request(void* requester, char* line, char* username)
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
            api__chat__message_envelope__free_unpacked(res_env, NULL);
    }
    else
    {
        printf("Użycie: /dm <użytkownik> <treść>\n");
    }
}

static void clean_up(void* requester, void* ctrl_pub, RequesterArgs* args)
{
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
