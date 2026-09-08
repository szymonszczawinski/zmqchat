#include "chat_client_controller.h"
#include "chat_client_common.h"

#include "client/chat_client_socket.h"
#include "generated/chat.pb-c.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <zmq.h>
#include <poll.h>

// Helper wysyłający i odbierający na REQ
static Api__Chat__MessageEnvelope* send_and_recv_env(socket_req_t*               socket_req_chat,
                                                     Api__Chat__MessageEnvelope* envelope);

static void handle_rooms_request(socket_req_t* socket_req_chat, socket_pair_t* socket_pair_ui_command);

static void handle_room_join_request(socket_req_t*  socket_req_chat,
                                     socket_pair_t* socket_pair_receiver_ctrl,
                                     socket_pair_t* socket_pair_ui_command,
                                     char*          line,
                                     char*          username);

static void handle_room_leave_request(socket_req_t*  socket_req_chat,
                                      socket_pair_t* socket_pair_receiver_ctrl,
                                      socket_pair_t* socket_pair_ui_command,
                                      char*          line,
                                      char*          username);

static void handle_message_room_request(socket_req_t*  socket_req_chat,
                                        socket_pair_t* socket_pair_ui_command,
                                        char*          line,
                                        char*          username);

static void handle_message_direct_request(socket_req_t*  socket_req_chat,
                                          socket_pair_t* socket_pair_ui_command,
                                          char*          line,
                                          char*          username);

static void clean_up(socket_sub_t*  socket_sub_shutdown,
                     socket_req_t*  socket_req_chat,
                     socket_pair_t* socket_pair_receiver_ctrl,
                     socket_pair_t* socket_pair_ui_command);

void logout(socket_req_t* socket_req_chat, char* username)
{
    printf("[ZMQClient][controller thread] logout\n");
    Api__Chat__LeaveRoomRequest leave_req = API__CHAT__LEAVE_ROOM_REQUEST__INIT;
    leave_req.room_name                   = CHAT_ROOM_GENERAL;
    leave_req.username                    = username;

    Api__Chat__MessageEnvelope leave_req_env = API__CHAT__MESSAGE_ENVELOPE__INIT;
    leave_req_env.message_id                 = "shutdown-leave";
    leave_req_env.payload_case               = API__CHAT__MESSAGE_ENVELOPE__PAYLOAD_LEAVE_ROOM_REQ;
    leave_req_env.leave_room_req             = &leave_req;

    Api__Chat__MessageEnvelope* resp_leave = send_and_recv_env(socket_req_chat, &leave_req_env);
    if (resp_leave)
    {
        printf("[ZMQClient][controller thread] Server acknowledged leave general.\n");
        api__chat__message_envelope__free_unpacked(resp_leave, NULL);
    }
}

void* routine_chat_controller(void* arg)
{
    printf("[ZMQClient][controller thread] start socket_req_chat_routine\n");

    ControllerArgs* args     = (ControllerArgs*)arg;
    void*           context  = args->context;
    char*           username = args->username;

    // Control socket INPROC (SUB for kill/stop signal)
    socket_sub_t* socket_sub_shutdown = socket_sub_new(context);
    if (socket_sub_connect(socket_sub_shutdown, INPROC_SHUTDOWN_ADDR) == -1)
    {
        printf("[ZMQClient][controller thread] shutdown socket connect failed: %s\n", strerror(errno));
        clean_up(socket_sub_shutdown, NULL, NULL, NULL);
        return NULL;
    }
    if (socket_sub_subscribe(socket_sub_shutdown, "") != 0)
    {
        printf("[ZMQClient][controller thread] shutdown socket subscribe failed: %s\n", strerror(errno));
        clean_up(socket_sub_shutdown, NULL, NULL, NULL);
        return NULL;
    }
    // REQ socket to server
    socket_req_t* socket_req_chat = socket_req_new(context);
    if (socket_req_connect(socket_req_chat, CHAT_SERVER_REQ_ADDRESS) != 0)
    {
        perror("[ZMQClient][controller thread] REQ connection error");
        clean_up(socket_sub_shutdown, socket_req_chat, NULL, NULL);
        return NULL;
    }

    // PAIR socket to receiver
    socket_pair_t* socket_pair_receiver_ctrl = socket_pair_new(context);
    if (socket_pair_bind(socket_pair_receiver_ctrl, CONTROLLER_RECEIVER_COMMAND_ADDRESS) != 0)
    {
        perror("[ZMQClient][controller thread] bind inproc://receiver-control error");
        clean_up(socket_sub_shutdown, socket_req_chat, socket_pair_receiver_ctrl, NULL);
        return NULL;
    }

    // PAIR socket to UI
    socket_pair_t* socket_pair_ui_command = socket_pair_new(context);
    if (socket_pair_bind(socket_pair_ui_command, CONROLLER_UI_COMMAND_ADDRESS) != 0)
    {
        perror("[ZMQClient][controller thread] bind inproc://command-socket error");
        clean_up(socket_sub_shutdown, socket_req_chat, socket_pair_receiver_ctrl, socket_pair_ui_command);
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

    Api__Chat__MessageEnvelope* resp = send_and_recv_env(socket_req_chat, &env);
    if (!resp || resp->payload_case != API__CHAT__MESSAGE_ENVELOPE__PAYLOAD_LOGIN_RESP
        || resp->login_resp->status != API__CHAT__STATUS__STATUS_OK)
    {
        printf("[ZMQClient][controller thread] login failed!\n");
        clean_up(socket_sub_shutdown, socket_req_chat, socket_pair_receiver_ctrl, socket_pair_ui_command);
        return NULL;
    }
    api__chat__message_envelope__free_unpacked(resp, NULL);
    printf("[ZMQClient][controller thread] login success as '%s'!\n", username);

    zmq_pollitem_t items[] = { { socket_sub_get_raw(socket_sub_shutdown), 0, ZMQ_POLLIN, 0 },
                               { socket_pair_get_raw(socket_pair_ui_command), 0, ZMQ_POLLIN, 0 } };

    //  5. Pętla obsługi poleceń z wątku UI
    printf("[ZMQClient][controller thread] loop start!\n");
    while (1)
    {
        // Wątek bezpiecznie "śpi", czekając na jedno z dwóch zdarzeń
        printf("[ZMQClient][controller thread] waiting...\n");
        int rc = zmq_poll(items, 2, -1);
        if (rc < 0)
        {
            if (errno == EINTR)
            {
                continue;  // Przerwanie sygnałem systemowym - ponawiamy poll
            }
            perror("[ZMQClient][controller thread] zmq_poll error");
            break;
        }

        // --- A. ODBIERANIE SYGNAŁU SHUTDOWN ("KILL") ---
        if (items[0].revents & ZMQ_POLLIN)
        {
            char shutdown_msg[10] = { 0 };
            int  bytes            = socket_sub_recv(socket_sub_shutdown, shutdown_msg, sizeof(shutdown_msg) - 1, 0);
            if (bytes > 0)
            {
                shutdown_msg[bytes] = '\0';
                if (strcmp(shutdown_msg, CHAT_MESSAGE_KILL) == 0)
                {
                    printf("[ZMQClient][controller thread] Received KILL signal. Shutting down cleanly...\n");
                    break;
                }
            }
        }
        // --- B. ODBIERANIE KOMEND Z WĄTKU UI (ZMQ_PAIR) ---
        if (items[1].revents & ZMQ_POLLIN)
        {
            char cmd_buf[512];
            int  bytes = socket_pair_recv(socket_pair_ui_command, cmd_buf, sizeof(cmd_buf) - 1, 0);
            if (bytes <= 0)
            {
                break;
            }
            cmd_buf[bytes] = '\0';

            if (strcmp(cmd_buf, CHAT_COMMAND_QUIT) == 0)
            {
                break;
            }

            if (strcmp(cmd_buf, CHAT_COMMAND_ROOMS) == 0)
            {
                handle_rooms_request(socket_req_chat, socket_pair_ui_command);
            }
            else if (strncmp(cmd_buf, CHAT_COMMAND_JOIN, 5) == 0)
            {
                handle_room_join_request(
                    socket_req_chat, socket_pair_receiver_ctrl, socket_pair_ui_command, cmd_buf, username);
            }
            else if (strncmp(cmd_buf, CHAT_COMMAND_LEAVE, 6) == 0)
            {
                handle_room_leave_request(
                    socket_req_chat, socket_pair_receiver_ctrl, socket_pair_ui_command, cmd_buf, username);
            }
            else if (strncmp(cmd_buf, CHAT_COMMAND_MSG, 4) == 0)
            {
                handle_message_room_request(socket_req_chat, socket_pair_ui_command, cmd_buf, username);
            }
            else if (strncmp(cmd_buf, CHAT_COMMAND_DM, 3) == 0)
            {
                handle_message_direct_request(socket_req_chat, socket_pair_ui_command, cmd_buf, username);
            }
            else
            {
                socket_pair_send(socket_pair_ui_command, "Nieznana komenda.", 17, 0);
            }
        }
    }

    logout(socket_req_chat, username);

    clean_up(socket_sub_shutdown, socket_req_chat, socket_pair_receiver_ctrl, socket_pair_ui_command);
    printf("[ZMQClient][controller thread] exit\n");
    return NULL;
}

static void handle_rooms_request(socket_req_t* socket_req_chat, socket_pair_t* socket_pair_ui_command)
{
    Api__Chat__ListRoomsRequest room_list_req     = API__CHAT__LIST_ROOMS_REQUEST__INIT;
    Api__Chat__MessageEnvelope  room_list_req_env = API__CHAT__MESSAGE_ENVELOPE__INIT;
    room_list_req_env.message_id                  = "cmd-rooms";
    room_list_req_env.payload_case                = API__CHAT__MESSAGE_ENVELOPE__PAYLOAD_LIST_ROOMS_REQ;
    room_list_req_env.list_rooms_req              = &room_list_req;

    printf("[ZMQClient][controller thread] sending room list request\n");
    Api__Chat__MessageEnvelope* room_list_res_env = send_and_recv_env(socket_req_chat, &room_list_req_env);
    printf("[ZMQClient][controller thread] received room list response\n");

    char out_buf[1024] = "Nie udało się pobrać listy pokojów.";
    if (room_list_res_env && room_list_res_env->payload_case == API__CHAT__MESSAGE_ENVELOPE__PAYLOAD_LIST_ROOMS_RESP)
    {
        int offset = snprintf(
            out_buf, sizeof(out_buf), "--- Lista Pokojów (%zu) ---\n", room_list_res_env->list_rooms_resp->n_rooms);
        printf("[ZMQClient][controller thread] received rooms number: %zu\n",
               room_list_res_env->list_rooms_resp->n_rooms);
        for (size_t i = 0; i < room_list_res_env->list_rooms_resp->n_rooms; i++)
        {
            Api__Chat__RoomInfo* r = room_list_res_env->list_rooms_resp->rooms[i];
            offset += snprintf(out_buf + offset,
                               sizeof(out_buf) - offset,
                               " * Pokój: #%-10s | Członków: %d\n",
                               r->name,
                               r->member_count);
        }
    }

    printf("[ZMQClient][controller thread] forward room list to UI: %s\n", out_buf);
    socket_pair_send(socket_pair_ui_command, out_buf, strlen(out_buf), 0);

    if (room_list_res_env)
    {
        api__chat__message_envelope__free_unpacked(room_list_res_env, NULL);
    }
}

static void handle_room_join_request(socket_req_t*  socket_req_chat,
                                     socket_pair_t* socket_pair_receiver_ctrl,
                                     socket_pair_t* socket_pair_ui_command,
                                     char*          line,
                                     char*          username)
{
    char* room_name = line + 6;

    Api__Chat__JoinRoomRequest join_req = API__CHAT__JOIN_ROOM_REQUEST__INIT;
    join_req.room_name                  = room_name;
    join_req.username                   = username;

    Api__Chat__MessageEnvelope req_env = API__CHAT__MESSAGE_ENVELOPE__INIT;
    req_env.message_id                 = "cmd-join";
    req_env.payload_case               = API__CHAT__MESSAGE_ENVELOPE__PAYLOAD_JOIN_ROOM_REQ;
    req_env.join_room_req              = &join_req;

    Api__Chat__MessageEnvelope* res_env = send_and_recv_env(socket_req_chat, &req_env);

    char out_buf[256] = "Błąd dołączania do pokoju.";
    if (res_env && res_env->payload_case == API__CHAT__MESSAGE_ENVELOPE__PAYLOAD_ACK)
    {
        snprintf(out_buf, sizeof(out_buf), "[SERWER]: %s", res_env->ack->message);

        // Wysyłamy komendę do wątku SUB po szynie inproc
        char ctrl_cmd[128];
        snprintf(ctrl_cmd, sizeof(ctrl_cmd), CHAT_MESSAGE_JOIN_ROOM, room_name);
        socket_pair_send(socket_pair_receiver_ctrl, ctrl_cmd, strlen(ctrl_cmd), 0);
    }

    socket_pair_send(socket_pair_ui_command, out_buf, strlen(out_buf), 0);

    if (res_env)
    {
        api__chat__message_envelope__free_unpacked(res_env, NULL);
    }
}

static Api__Chat__MessageEnvelope* send_and_recv_env(socket_req_t*               socket_req_chat,
                                                     Api__Chat__MessageEnvelope* envelope)
{
    size_t   envelope_packed_size = api__chat__message_envelope__get_packed_size(envelope);
    uint8_t* envelope_buffer      = malloc(envelope_packed_size);
    api__chat__message_envelope__pack(envelope, envelope_buffer);

    socket_req_send(socket_req_chat, envelope_buffer, envelope_packed_size, 0);
    free(envelope_buffer);

    uint8_t received_buffer[4096];
    int     received_bytes_number = socket_req_recv(socket_req_chat, received_buffer, sizeof(received_buffer), 0);
    if (received_bytes_number < 0)
        return NULL;

    return api__chat__message_envelope__unpack(NULL, received_bytes_number, received_buffer);
}

static void handle_room_leave_request(socket_req_t*  socket_req_chat,
                                      socket_pair_t* socket_pair_receiver_ctrl,
                                      socket_pair_t* socket_pair_ui_command,
                                      char*          line,
                                      char*          username)
{
    char* room_name = line + 7;

    Api__Chat__LeaveRoomRequest leave_req = API__CHAT__LEAVE_ROOM_REQUEST__INIT;
    leave_req.room_name                   = room_name;
    leave_req.username                    = username;

    Api__Chat__MessageEnvelope req_env = API__CHAT__MESSAGE_ENVELOPE__INIT;
    req_env.message_id                 = "cmd-leave";
    req_env.payload_case               = API__CHAT__MESSAGE_ENVELOPE__PAYLOAD_LEAVE_ROOM_REQ;
    req_env.leave_room_req             = &leave_req;

    Api__Chat__MessageEnvelope* res_env = send_and_recv_env(socket_req_chat, &req_env);

    char out_buf[256] = "Błąd opuszczania pokoju.";
    if (res_env && res_env->payload_case == API__CHAT__MESSAGE_ENVELOPE__PAYLOAD_ACK)
    {
        snprintf(out_buf, sizeof(out_buf), "[SERWER]: %s", res_env->ack->message);

        char ctrl_cmd[128];
        snprintf(ctrl_cmd, sizeof(ctrl_cmd), CHAT_MESSAGE_LEAVE_ROOM, room_name);
        socket_pair_send(socket_pair_receiver_ctrl, ctrl_cmd, strlen(ctrl_cmd), 0);
    }

    socket_pair_send(socket_pair_ui_command, out_buf, strlen(out_buf), 0);

    if (res_env)
    {
        api__chat__message_envelope__free_unpacked(res_env, NULL);
    }
}

static void handle_message_room_request(socket_req_t*  socket_req_chat,
                                        socket_pair_t* socket_pair_ui_command,
                                        char*          line,
                                        char*          username)
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

        Api__Chat__MessageEnvelope* res_env = send_and_recv_env(socket_req_chat, &req_env);
        if (res_env)
        {
            socket_pair_send(socket_pair_ui_command, "[ACK] Wiadomość wysłana", 22, 0);
            api__chat__message_envelope__free_unpacked(res_env, NULL);
        }
        else
        {
            socket_pair_send(socket_pair_ui_command, "[ERR] Błąd wysyłania wiadomości", 31, 0);
        }
    }
    else
    {
        socket_pair_send(socket_pair_ui_command, "Użycie: /msg <nazwa_pokoju> <treść>", 35, 0);
    }
}

static void handle_message_direct_request(socket_req_t*  socket_req_chat,
                                          socket_pair_t* socket_pair_ui_command,
                                          char*          line,
                                          char*          username)
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

        Api__Chat__MessageEnvelope* res_env = send_and_recv_env(socket_req_chat, &req_env);
        if (res_env)
        {
            socket_pair_send(socket_pair_ui_command, "[ACK] Wiadomość prywatna wysłana", 32, 0);
            api__chat__message_envelope__free_unpacked(res_env, NULL);
        }
        else
        {
            socket_pair_send(socket_pair_ui_command, "[ERR] Błąd wysyłania DM", 23, 0);
        }
    }
    else
    {
        socket_pair_send(socket_pair_ui_command, "Użycie: /dm <użytkownik> <treść>", 32, 0);
    }
}

static void clean_up(socket_sub_t*  socket_sub_shutdown,
                     socket_req_t*  socket_req_chat,
                     socket_pair_t* socket_pair_ui_command,
                     socket_pair_t* socket_pair_receiver_ctrl)
{
    socket_sub_destroy(&socket_sub_shutdown);
    socket_req_destroy(&socket_req_chat);
    socket_pair_destroy(&socket_pair_ui_command);
    socket_pair_destroy(&socket_pair_receiver_ctrl);
}
