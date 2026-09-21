#include "chat_client_controller.h"
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
#include <poll.h>

#define OK_ZMQ                 0
#define OK_ZMQ_QUIT            1001
#define ERROR_ZMQ_GENERAL      -1
#define ERROR_ZMQ_REQ_SEND     -1001
#define ERROR_ZMQ_REQ_RECV     -1002
#define ERROR_ZMQ_PAIR_UI_SEND -1003

// Helper wysyłający i odbierający na REQ
static Api__Chat__MessageEnvelope* send_and_recv_env(socket_req_t*               socket_req_chat,
                                                     Api__Chat__MessageEnvelope* envelope,
                                                     socket_push_t*              socket_push_logs);

static int handle_message_ui(socket_req_t*  socket_req_chat,
                             socket_pair_t* socket_pair_receiver_ctrl,
                             socket_pair_t* socket_pair_ui_command,
                             char*          username,
                             socket_push_t* socket_push_logs);

static int handle_room_list(socket_req_t*  socket_req_chat,
                            socket_pair_t* socket_pair_ui_command,
                            socket_push_t* socket_push_logs);

static int handle_room_join(socket_req_t*  socket_req_chat,
                            socket_pair_t* socket_pair_receiver_ctrl,
                            socket_pair_t* socket_pair_ui_command,
                            char*          line,
                            char*          username,
                            socket_push_t* socket_push_logs);

static int handle_room_leave(socket_req_t*  socket_req_chat,
                             socket_pair_t* socket_pair_receiver_ctrl,
                             socket_pair_t* socket_pair_ui_command,
                             char*          line,
                             char*          username,
                             socket_push_t* socket_push_logs);

static int handle_message_room(socket_req_t*  socket_req_chat,
                               socket_pair_t* socket_pair_ui_command,
                               char*          line,
                               char*          username,
                               socket_push_t* socket_push_logs);

static int handle_message_direct(socket_req_t*  socket_req_chat,
                                 socket_pair_t* socket_pair_ui_command,
                                 char*          line,
                                 char*          username,
                                 socket_push_t* socket_push_logs);

static void clean_up(socket_sub_t*  socket_sub_shutdown,
                     socket_req_t*  socket_req_chat,
                     socket_pair_t* socket_pair_receiver_ctrl,
                     socket_pair_t* socket_pair_ui_command);

static int handle_shutdown_message(socket_sub_t* socket_sub_shutdown, socket_push_t* socket_push_logs);

static int send_ping_message(socket_req_t* socket_req_chat, char* username, socket_push_t* socket_push_logs);

static int logout(socket_req_t* socket_req_chat, char* username, socket_push_t* socket_push_logs);

static int send_login_request(socket_sub_t*  socket_sub_shutdown,
                              socket_req_t*  socket_req_chat,
                              socket_pair_t* socket_pair_receiver_ctrl,
                              socket_pair_t* socket_pair_ui_command,
                              char*          username,
                              socket_push_t* socket_push_logs);

static void log_app(socket_push_t* socket_push_logs, const char* format, ...);

void* routine_chat_controller(void* arg)
{
    ControllerArgs* args     = (ControllerArgs*)arg;
    void*           context  = args->context;
    char*           username = args->username;

    socket_push_t* socket_push_logs = socket_push_new(context);
    if (socket_push_connect(socket_push_logs, APP_LOGS_ADDRESS) != 0)
    {
        perror("[ZMQClient][controller thread] connect inproc://app_log error");
        socket_push_destroy(&socket_push_logs);
        return NULL;
    }
    log_app(socket_push_logs, "[ZMQClient][controller thread] start socket_req_chat_routine\n");

    // Control socket INPROC (SUB for kill/stop signal)
    socket_sub_t* socket_sub_shutdown = socket_sub_new(context);
    if (socket_sub_connect(socket_sub_shutdown, INPROC_SHUTDOWN_ADDR) == -1)
    {
        log_app(
            socket_push_logs, "[ZMQClient][controller thread] shutdown socket connect failed: %s\n", strerror(errno));
        clean_up(socket_sub_shutdown, NULL, NULL, NULL);
        return NULL;
    }
    if (socket_sub_subscribe(socket_sub_shutdown, "") != 0)
    {
        log_app(
            socket_push_logs, "[ZMQClient][controller thread] shutdown socket subscribe failed: %s\n", strerror(errno));
        clean_up(socket_sub_shutdown, NULL, NULL, NULL);
        return NULL;
    }
    // REQ socket to server
    socket_req_t* socket_req_chat = socket_req_new(context);
    if (socket_req_connect(socket_req_chat, CHAT_SERVER_REQ_ADDRESS) != 0)
    {
        log_app(socket_push_logs, "[ZMQClient][controller thread] REQ connection error");
        clean_up(socket_sub_shutdown, socket_req_chat, NULL, NULL);
        return NULL;
    }

    // PAIR socket to receiver
    socket_pair_t* socket_pair_receiver_ctrl = socket_pair_new(context);
    if (socket_pair_bind(socket_pair_receiver_ctrl, CONTROLLER_RECEIVER_COMMAND_ADDRESS) != 0)
    {
        log_app(socket_push_logs, "[ZMQClient][controller thread] bind inproc://receiver-control error");
        clean_up(socket_sub_shutdown, socket_req_chat, socket_pair_receiver_ctrl, NULL);
        return NULL;
    }

    // PAIR socket to UI
    socket_pair_t* socket_pair_ui_command = socket_pair_new(context);
    if (socket_pair_bind(socket_pair_ui_command, CONROLLER_UI_COMMAND_ADDRESS) != 0)
    {
        log_app(socket_push_logs, "[ZMQClient][controller thread] bind inproc://command-socket error");
        clean_up(socket_sub_shutdown, socket_req_chat, socket_pair_receiver_ctrl, socket_pair_ui_command);
        return NULL;
    }

    //  auto login on start
    if (send_login_request(socket_sub_shutdown,
                           socket_req_chat,
                           socket_pair_receiver_ctrl,
                           socket_pair_ui_command,
                           username,
                           socket_push_logs)
        < 0)
    {
        return NULL;
    }
    log_app(socket_push_logs, "[ZMQClient][controller thread] login success as '%s'!\n", username);

    zmq_pollitem_t items[] = { { socket_sub_get_raw(socket_sub_shutdown), 0, ZMQ_POLLIN, 0 },
                               { socket_pair_get_raw(socket_pair_ui_command), 0, ZMQ_POLLIN, 0 } };

    //  main loop handling shutdown, messages from UI and heartbeat
    log_app(socket_push_logs, "[ZMQClient][controller thread] loop start!\n");
    while (1)
    {
        // Wątek bezpiecznie "śpi", czekając na jedno z dwóch zdarzeń
        log_app(socket_push_logs, "[ZMQClient][controller thread] waiting...\n");
        int rc = zmq_poll(items, 2, 2000);
        if (rc < 0)
        {
            if (errno == EINTR)
            {
                continue;  // Przerwanie sygnałem systemowym - ponawiamy poll
            }
            log_app(socket_push_logs, "[ZMQClient][controller thread] zmq_poll error");
            break;
        }

        // --- handle SHUTDOWN ("KILL") ---
        if (items[0].revents & ZMQ_POLLIN)
        {
            if (handle_shutdown_message(socket_sub_shutdown, socket_push_logs) < 0)
            {
                break;
            }
        }
        // --- handle command from UI (ZMQ_PAIR) ---
        if (items[1].revents & ZMQ_POLLIN)
        {
            int result = handle_message_ui(
                socket_req_chat, socket_pair_receiver_ctrl, socket_pair_ui_command, username, socket_push_logs);
            if (result < 0)
            {
                break;
            }
            if (result == OK_ZMQ_QUIT)
            {
                break;
            }
        }
        else if (rc == 0)
        {
            // --- TIMEOUT (2s) -> send PING ---
            send_ping_message(socket_req_chat, username, socket_push_logs);
        }
    }

    logout(socket_req_chat, username, socket_push_logs);

    clean_up(socket_sub_shutdown, socket_req_chat, socket_pair_receiver_ctrl, socket_pair_ui_command);
    log_app(socket_push_logs, "[ZMQClient][controller thread] exit\n");
    socket_push_destroy(&socket_push_logs);
    return NULL;
}

static int handle_room_list(socket_req_t*  socket_req_chat,
                            socket_pair_t* socket_pair_ui_command,
                            socket_push_t* socket_push_logs)
{
    Api__Chat__ListRoomsRequest room_list_req     = API__CHAT__LIST_ROOMS_REQUEST__INIT;
    Api__Chat__MessageEnvelope  room_list_req_env = API__CHAT__MESSAGE_ENVELOPE__INIT;
    room_list_req_env.message_id                  = "cmd-rooms";
    room_list_req_env.payload_case                = API__CHAT__MESSAGE_ENVELOPE__PAYLOAD_LIST_ROOMS_REQ;
    room_list_req_env.list_rooms_req              = &room_list_req;

    log_app(socket_push_logs, "[ZMQClient][controller thread] sending room list request\n");
    Api__Chat__MessageEnvelope* room_list_res_env
        = send_and_recv_env(socket_req_chat, &room_list_req_env, socket_push_logs);
    log_app(socket_push_logs, "[ZMQClient][controller thread] received room list response\n");

    char out_buf[1024] = "could not get rooms list";
    if (room_list_res_env)
    {
        if (room_list_res_env->payload_case == API__CHAT__MESSAGE_ENVELOPE__PAYLOAD_LIST_ROOMS_RESP)
        {
            int offset = snprintf(
                out_buf, sizeof(out_buf), "--- Lista Pokojów (%zu) ---\n", room_list_res_env->list_rooms_resp->n_rooms);
            log_app(socket_push_logs,
                    "[ZMQClient][controller thread] received rooms number: %zu\n",
                    room_list_res_env->list_rooms_resp->n_rooms);
            for (size_t i = 0; i < room_list_res_env->list_rooms_resp->n_rooms; i++)
            {
                Api__Chat__RoomInfo* r = room_list_res_env->list_rooms_resp->rooms[i];
                offset += snprintf(out_buf + offset,
                                   sizeof(out_buf) - offset,
                                   " * room: #%-10s | members: %d\n",
                                   r->name,
                                   r->member_count);
            }
        }

        log_app(socket_push_logs, "[ZMQClient][controller thread] forward room list to UI: %s\n", out_buf);
        socket_pair_send(socket_pair_ui_command, out_buf, strlen(out_buf), 0);

        api__chat__message_envelope__free_unpacked(room_list_res_env, NULL);
        return OK_ZMQ;
    }
    else
    {
        log_app(socket_push_logs,
                "[ZMQClient][controller thread][handle_rooms_request][REQ][CHAT] error sending message : %s\n",
                strerror(errno));
        return ERROR_ZMQ_REQ_SEND;
    }
}

static int handle_room_join(socket_req_t*  socket_req_chat,
                            socket_pair_t* socket_pair_receiver_ctrl,
                            socket_pair_t* socket_pair_ui_command,
                            char*          line,
                            char*          username,
                            socket_push_t* socket_push_logs)
{
    char* room_name = line + 6;

    Api__Chat__JoinRoomRequest join_req = API__CHAT__JOIN_ROOM_REQUEST__INIT;
    join_req.room_name                  = room_name;
    join_req.username                   = username;

    Api__Chat__MessageEnvelope req_env = API__CHAT__MESSAGE_ENVELOPE__INIT;
    req_env.message_id                 = "cmd-join";
    req_env.payload_case               = API__CHAT__MESSAGE_ENVELOPE__PAYLOAD_JOIN_ROOM_REQ;
    req_env.join_room_req              = &join_req;

    Api__Chat__MessageEnvelope* res_env = send_and_recv_env(socket_req_chat, &req_env, socket_push_logs);
    if (res_env)
    {

        char out_buf[256] = "Błąd dołączania do pokoju.";
        if (res_env->payload_case == API__CHAT__MESSAGE_ENVELOPE__PAYLOAD_ACK)
        {
            snprintf(out_buf, sizeof(out_buf), "[SERWER]: %s", res_env->ack->message);

            // send command to SUB via inproc
            char ctrl_cmd[128];
            snprintf(ctrl_cmd, sizeof(ctrl_cmd), CHAT_MESSAGE_JOIN_ROOM, room_name);
            if (socket_pair_send(socket_pair_receiver_ctrl, ctrl_cmd, strlen(ctrl_cmd), 0) < 0)
            {

                log_app(socket_push_logs,
                        "[ZMQClient][controller thread][handle_room_join_request][PAIR][receiver] error sending "
                        "message: %s\n",
                        strerror(errno));
            }
        }

        if (socket_pair_send(socket_pair_ui_command, out_buf, strlen(out_buf), 0) < 0)
        {

            log_app(socket_push_logs,
                    "[ZMQClient][controller thread][handle_room_join_request][PAIR][ui] error sending "
                    "message: %s\n",
                    strerror(errno));
        }

        api__chat__message_envelope__free_unpacked(res_env, NULL);
        return OK_ZMQ;
    }
    else
    {
        log_app(socket_push_logs,
                "[ZMQClient][controller thread][handle_room_join_request][REQ][CHAT] error sending message : %s\n",
                strerror(errno));
        return ERROR_ZMQ_REQ_SEND;
    }
}

static Api__Chat__MessageEnvelope* send_and_recv_env(socket_req_t*               socket_req_chat,
                                                     Api__Chat__MessageEnvelope* envelope,
                                                     socket_push_t*              socket_push_logs)
{
    size_t   envelope_packed_size = api__chat__message_envelope__get_packed_size(envelope);
    uint8_t* envelope_buffer      = malloc(envelope_packed_size);
    api__chat__message_envelope__pack(envelope, envelope_buffer);

    int rc = socket_req_send(socket_req_chat, envelope_buffer, envelope_packed_size, 0);
    free(envelope_buffer);

    if (rc < 0)
    {
        log_app(socket_push_logs,
                "[ZMQClient][controller thread][send_and_recv_env] error sending message: %s\n",
                strerror(errno));
        return NULL;
    }

    uint8_t received_buffer[4096];
    int     received_bytes_number = socket_req_recv(socket_req_chat, received_buffer, sizeof(received_buffer), 0);
    if (received_bytes_number < 0)
    {
        log_app(socket_push_logs,
                "[ZMQClient][controller thread][send_and_recv_env] error receiving message: %s\n",
                strerror(errno));
        return NULL;
    }

    return api__chat__message_envelope__unpack(NULL, received_bytes_number, received_buffer);
}

static int handle_room_leave(socket_req_t*  socket_req_chat,
                             socket_pair_t* socket_pair_receiver_ctrl,
                             socket_pair_t* socket_pair_ui_command,
                             char*          line,
                             char*          username,
                             socket_push_t* socket_push_logs)
{
    char* room_name = line + 7;

    Api__Chat__LeaveRoomRequest leave_req = API__CHAT__LEAVE_ROOM_REQUEST__INIT;
    leave_req.room_name                   = room_name;
    leave_req.username                    = username;

    Api__Chat__MessageEnvelope req_env = API__CHAT__MESSAGE_ENVELOPE__INIT;
    req_env.message_id                 = "cmd-leave";
    req_env.payload_case               = API__CHAT__MESSAGE_ENVELOPE__PAYLOAD_LEAVE_ROOM_REQ;
    req_env.leave_room_req             = &leave_req;

    Api__Chat__MessageEnvelope* res_env = send_and_recv_env(socket_req_chat, &req_env, socket_push_logs);

    char out_buf[256] = "error leaving room";
    if (res_env)
    {
        if (res_env->payload_case == API__CHAT__MESSAGE_ENVELOPE__PAYLOAD_ACK)
        {
            snprintf(out_buf, sizeof(out_buf), "[SERWER]: %s", res_env->ack->message);

            char ctrl_cmd[128];
            snprintf(ctrl_cmd, sizeof(ctrl_cmd), CHAT_MESSAGE_LEAVE_ROOM, room_name);
            socket_pair_send(socket_pair_receiver_ctrl, ctrl_cmd, strlen(ctrl_cmd), 0);
        }

        socket_pair_send(socket_pair_ui_command, out_buf, strlen(out_buf), 0);

        api__chat__message_envelope__free_unpacked(res_env, NULL);
        return OK_ZMQ;
    }
    else
    {
        log_app(socket_push_logs,
                "[ZMQClient][controller thread][handle_room_leave_request] error sending message: %s\n",
                strerror(errno));
        return ERROR_ZMQ_REQ_SEND;
    }
}

static int handle_message_room(socket_req_t*  socket_req_chat,
                               socket_pair_t* socket_pair_ui_command,
                               char*          line,
                               char*          username,
                               socket_push_t* socket_push_logs)
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

        Api__Chat__MessageEnvelope* res_env = send_and_recv_env(socket_req_chat, &req_env, socket_push_logs);
        if (res_env)
        {
            socket_pair_send(socket_pair_ui_command, CHAT_MESSAGE_SEND_ACK, strlen(CHAT_MESSAGE_SEND_ACK), 0);
            api__chat__message_envelope__free_unpacked(res_env, NULL);
        }
        else
        {
            log_app(socket_push_logs,
                    "[ZMQClient][controller thread][handle_message_room_request] error sending message: %s\n",
                    strerror(errno));
            socket_pair_send(socket_pair_ui_command, "[ERR] Błąd wysyłania wiadomości", 31, 0);
        }
    }
    else
    {
        socket_pair_send(socket_pair_ui_command, "Użycie: /msg <nazwa_pokoju> <treść>", 35, 0);
    }
    return OK_ZMQ;
}

static int handle_message_direct(socket_req_t*  socket_req_chat,
                                 socket_pair_t* socket_pair_ui_command,
                                 char*          line,
                                 char*          username,
                                 socket_push_t* socket_push_logs)
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

        Api__Chat__MessageEnvelope* res_env = send_and_recv_env(socket_req_chat, &req_env, socket_push_logs);
        if (res_env)
        {
            socket_pair_send(socket_pair_ui_command, "[ACK] Wiadomość prywatna wysłana", 32, 0);
            api__chat__message_envelope__free_unpacked(res_env, NULL);
        }
        else
        {
            log_app(socket_push_logs,
                    "[ZMQClient][controller thread][handle_message_direct_request] error sending message: %s\n",
                    strerror(errno));
            socket_pair_send(socket_pair_ui_command, "[ERR] Błąd wysyłania DM", 23, 0);
        }
    }
    else
    {
        socket_pair_send(socket_pair_ui_command, "Użycie: /dm <użytkownik> <treść>", 32, 0);
    }
    return OK_ZMQ;
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

static int logout(socket_req_t* socket_req_chat, char* username, socket_push_t* socket_push_logs)
{
    log_app(socket_push_logs, "[ZMQClient][controller thread] logout\n");
    Api__Chat__LogoutRequest logout_req = API__CHAT__LOGOUT_REQUEST__INIT;
    logout_req.username                 = username;

    Api__Chat__MessageEnvelope logout_req_env = API__CHAT__MESSAGE_ENVELOPE__INIT;
    logout_req_env.message_id                 = "cmd-logout";
    logout_req_env.payload_case               = API__CHAT__MESSAGE_ENVELOPE__PAYLOAD_LOGOUT_REQ;
    logout_req_env.logout_req                 = &logout_req;

    // Wysyłamy żądanie wylogowania do serwera przez gniazdo REQ
    Api__Chat__MessageEnvelope* res_env = send_and_recv_env(socket_req_chat, &logout_req_env, socket_push_logs);

    if (res_env && res_env->payload_case == API__CHAT__MESSAGE_ENVELOPE__PAYLOAD_ACK)
    {
        log_app(socket_push_logs, "[ZMQClient][controller thread][logout] Server acknowledged leave general.\n");
        api__chat__message_envelope__free_unpacked(res_env, NULL);
        return OK_ZMQ;
    }
    else
    {
        log_app(
            socket_push_logs, "[ZMQClient][controller thread][logout] error sending message: %s\n", strerror(errno));
        return ERROR_ZMQ_REQ_SEND;
    }
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
            log_app(socket_push_logs,
                    "[ZMQClient][controller thread] received KILL signal. shutting down cleanly...\n");
            return -1;
        }
    }
    return OK_ZMQ;
}

static int send_ping_message(socket_req_t* socket_req_chat, char* username, socket_push_t* socket_push_logs)
{
    Api__Chat__HeartbeatPing ping = API__CHAT__HEARTBEAT_PING__INIT;
    ping.username                 = username;
    ping.timestamp                = (int64_t)time(NULL) * 1000;

    Api__Chat__MessageEnvelope env_ping = API__CHAT__MESSAGE_ENVELOPE__INIT;
    env_ping.message_id                 = "heartbeat-ping";
    env_ping.payload_case               = API__CHAT__MESSAGE_ENVELOPE__PAYLOAD_HEARTBEAT;
    env_ping.heartbeat                  = &ping;
    log_app(socket_push_logs, "[ZMQClient][controller thread][ping] sending...\n");
    Api__Chat__MessageEnvelope* resp = send_and_recv_env(socket_req_chat, &env_ping, socket_push_logs);
    if (resp)
    {
        log_app(socket_push_logs, "[ZMQClient][controller thread][ping] pong received...\n");
        api__chat__message_envelope__free_unpacked(resp, NULL);
        return OK_ZMQ;
    }
    else
    {
        log_app(socket_push_logs, "[ZMQClient][controller thread][ping] error sending message: %s\n", strerror(errno));
        return ERROR_ZMQ_REQ_SEND;
    }
}

static int handle_message_ui(socket_req_t*  socket_req_chat,
                             socket_pair_t* socket_pair_receiver_ctrl,
                             socket_pair_t* socket_pair_ui_command,
                             char*          username,
                             socket_push_t* socket_push_logs)
{
    char cmd_buf[512];
    int  bytes = socket_pair_recv(socket_pair_ui_command, cmd_buf, sizeof(cmd_buf) - 1, 0);
    if (bytes <= 0)
    {
        return ERROR_ZMQ_GENERAL;
    }
    cmd_buf[bytes] = '\0';

    if (strcmp(cmd_buf, CHAT_COMMAND_QUIT) == 0)
    {
        log_app(socket_push_logs, "[ZMQClient][controller thread] receive 'quit' command\n");
        return OK_ZMQ_QUIT;
    }

    if (strcmp(cmd_buf, CHAT_COMMAND_ROOMS) == 0)
    {
        return handle_room_list(socket_req_chat, socket_pair_ui_command, socket_push_logs);
    }
    else if (strncmp(cmd_buf, CHAT_COMMAND_JOIN, 5) == 0)
    {
        return handle_room_join(
            socket_req_chat, socket_pair_receiver_ctrl, socket_pair_ui_command, cmd_buf, username, socket_push_logs);
    }
    else if (strncmp(cmd_buf, CHAT_COMMAND_LEAVE, 6) == 0)
    {
        return handle_room_leave(
            socket_req_chat, socket_pair_receiver_ctrl, socket_pair_ui_command, cmd_buf, username, socket_push_logs);
    }
    else if (strncmp(cmd_buf, CHAT_COMMAND_MSG, 4) == 0)
    {
        return handle_message_room(socket_req_chat, socket_pair_ui_command, cmd_buf, username, socket_push_logs);
    }
    else if (strncmp(cmd_buf, CHAT_COMMAND_DM, 3) == 0)
    {
        return handle_message_direct(socket_req_chat, socket_pair_ui_command, cmd_buf, username, socket_push_logs);
    }
    else
    {
        return socket_pair_send(socket_pair_ui_command, "Nieznana komenda.", 17, 0);
    }
    return OK_ZMQ;
}

static int send_login_request(socket_sub_t*  socket_sub_shutdown,
                              socket_req_t*  socket_req_chat,
                              socket_pair_t* socket_pair_receiver_ctrl,
                              socket_pair_t* socket_pair_ui_command,
                              char*          username,
                              socket_push_t* socket_push_logs)
{
    Api__Chat__LoginRequest login_req = API__CHAT__LOGIN_REQUEST__INIT;
    login_req.username                = username;
    login_req.client_version          = "1.0.0";

    Api__Chat__MessageEnvelope env = API__CHAT__MESSAGE_ENVELOPE__INIT;
    env.message_id                 = "req-login";
    env.payload_case               = API__CHAT__MESSAGE_ENVELOPE__PAYLOAD_LOGIN_REQ;
    env.login_req                  = &login_req;

    Api__Chat__MessageEnvelope* resp = send_and_recv_env(socket_req_chat, &env, socket_push_logs);
    if (!resp || resp->payload_case != API__CHAT__MESSAGE_ENVELOPE__PAYLOAD_LOGIN_RESP
        || resp->login_resp->status != API__CHAT__STATUS__STATUS_OK)
    {
        log_app(socket_push_logs, "[ZMQClient][controller thread][login] login failed!\n");
        clean_up(socket_sub_shutdown, socket_req_chat, socket_pair_receiver_ctrl, socket_pair_ui_command);
        return ERROR_ZMQ_REQ_SEND;
    }
    api__chat__message_envelope__free_unpacked(resp, NULL);
    return OK_ZMQ;
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
