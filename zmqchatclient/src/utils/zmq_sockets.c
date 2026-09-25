#include "zmq_sockets.h"
#include <stdlib.h>
#include <string.h>
#include <zmq.h>
#include "utils/zmq_messages.h"
#include "utils/zmq_utils.h"

// inner structs (hidden for .h file)
struct socket_sub
{
    void* zmq_socket;
};

struct socket_pub
{
    void* zmq_socket;
};

struct socket_req
{
    void* zmq_socket;
};

struct socket_rep
{
    void* zmq_socket;
};

struct socket_pair
{
    void* zmq_socket;
};

struct socket_push
{
    void* zmq_socket;
};

struct socket_pull
{
    void* zmq_socket;
};

struct socket_router
{
    void* zmq_socket;
};

struct socket_dealer
{
    void* zmq_socket;
};

// MACRO for new/destroy for all socket types
#define IMPL_SOCKET_LIFECYCLE(type_name, zmq_type)                                         \
    type_name##_t* type_name##_new(void* zmq_context)                                      \
    {                                                                                      \
        if (!zmq_context)                                                                  \
            return NULL;                                                                   \
        type_name##_t* self = malloc(sizeof(type_name##_t));                               \
        if (!self)                                                                         \
        {                                                                                  \
            printf("E: allocate memory for %s failed: %s\n", #type_name, strerror(errno)); \
            return NULL;                                                                   \
        }                                                                                  \
        self->zmq_socket = zmq_socket(zmq_context, zmq_type);                              \
        if (!self->zmq_socket)                                                             \
        {                                                                                  \
            printf("E: create socket for %s failed: %s\n", #type_name, strerror(errno));   \
            free(self);                                                                    \
            return NULL;                                                                   \
        }                                                                                  \
        return self;                                                                       \
    }                                                                                      \
    void type_name##_destroy(type_name##_t** self_p)                                       \
    {                                                                                      \
        if (self_p && *self_p)                                                             \
        {                                                                                  \
            int linger = 0;                                                                \
            zmq_setsockopt((*self_p)->zmq_socket, ZMQ_LINGER, &linger, sizeof(linger));    \
            zmq_close((*self_p)->zmq_socket);                                              \
            free(*self_p);                                                                 \
            *self_p = NULL;                                                                \
        }                                                                                  \
    }

#define SOCKET_GET_RAW(type_name)                  \
    void* type_name##_get_raw(type_name##_t* self) \
    {                                              \
        return self ? self->zmq_socket : NULL;     \
    }

#define SOCKET_CONNECT(type_name)                                                                        \
    int type_name##_connect(type_name##_t* self, const char* ep)                                         \
    {                                                                                                    \
        if (!self)                                                                                       \
        {                                                                                                \
            printf("E: socket connect for %s failed socket is NULL: %s\n", #type_name, strerror(errno)); \
        }                                                                                                \
        int rc = zmq_connect(self->zmq_socket, ep);                                                      \
        if (rc == -1)                                                                                    \
        {                                                                                                \
            printf("E: socket connect for %s failed: %s\n", #type_name, strerror(errno));                \
        }                                                                                                \
        return rc;                                                                                       \
    }

#define SOCKET_BIND(type_name)                                                                        \
    int type_name##_bind(type_name##_t* self, const char* ep)                                         \
    {                                                                                                 \
        if (!self)                                                                                    \
        {                                                                                             \
            printf("E: socket bind for %s failed socket is NULL: %s\n", #type_name, strerror(errno)); \
        }                                                                                             \
        int rc = zmq_bind(self->zmq_socket, ep);                                                      \
        if (rc == -1)                                                                                 \
        {                                                                                             \
            printf("E: socket bind for %s failed: %s\n", #type_name, strerror(errno));                \
        }                                                                                             \
        return rc;                                                                                    \
    }

#define SOCKET_RECEIVE(type_name)                                                                             \
    int type_name##_recv(type_name##_t* self, void* buf, size_t len, int flags)                               \
    {                                                                                                         \
        if (!self)                                                                                            \
        {                                                                                                     \
            printf("E: %s_recv for %s failed socket is NULL: %s\n", #type_name, #type_name, strerror(errno)); \
        }                                                                                                     \
        int rc = zmq_recv(self->zmq_socket, buf, len, flags);                                                 \
        if (rc == -1)                                                                                         \
        {                                                                                                     \
            printf("E: %s_recv for %s failed: %s\n", #type_name, #type_name, strerror(errno));                \
        }                                                                                                     \
        return rc;                                                                                            \
    }

#define SOCKET_SEND(type_name)                                                                                \
    int type_name##_send(type_name##_t* self, const void* buf, size_t len, int flags)                         \
    {                                                                                                         \
        if (!self)                                                                                            \
        {                                                                                                     \
            printf("E: %s_send for %s failed socket is NULL: %s\n", #type_name, #type_name, strerror(errno)); \
        }                                                                                                     \
        int rc = zmq_send(self->zmq_socket, buf, len, flags);                                                 \
        if (rc == -1)                                                                                         \
        {                                                                                                     \
            printf("E: %s_send for %s failed: %s\n", #type_name, #type_name, strerror(errno));                \
        }                                                                                                     \
        return rc;                                                                                            \
    }

#define SOCKET_MESSAGE_RECEIVE_AS_STRING(type_name)               \
    char* type_name##_recv_string(type_name##_t* self, int flags) \
    {                                                             \
        return recv_frame_as_string(self->zmq_socket, &flags);    \
    }

#define SOCKET_MESSAGE_SEND_AS_STRING(type_name)                                 \
    int type_name##_send_string(type_name##_t* self, const char* str, int flags) \
    {                                                                            \
        return send_frame_string(self->zmq_socket, str, flags);                  \
    }

#define SOCKET_MESSAGE_RECEIVE(type_name)                        \
    zmq_message_t* type_name##_message_recv(type_name##_t* self) \
    {                                                            \
        return message_recv(self->zmq_socket);                   \
    }

#define SOCKET_MESSAGE_SEND(type_name)                                               \
    int type_name##_message_send(type_name##_t* self, zmq_message_t* msg, int flags) \
    {                                                                                \
        return message_send(msg, self->zmq_socket, flags);                           \
    }

#define SOCKET_SET_ID(type_name)                                                                                     \
    int type_name##_set_id(type_name##_t* self, char* identity)                                                      \
    {                                                                                                                \
        if (!self)                                                                                                   \
        {                                                                                                            \
            printf("E: set socket id %s for %s failed socket is NULL: %s\n", identity, #type_name, strerror(errno)); \
            return -1;                                                                                               \
        }                                                                                                            \
        int rc = zmq_setsockopt(self->zmq_socket, ZMQ_IDENTITY, identity, strlen(identity));                         \
        if (rc == -1)                                                                                                \
        {                                                                                                            \
            printf("E: set socket id %s for %s failed: %s\n", identity, #type_name, strerror(errno));                \
        }                                                                                                            \
        return rc;                                                                                                   \
    }

// --- SUB ---

// new and destroy socket of given type
IMPL_SOCKET_LIFECYCLE(socket_sub, ZMQ_SUB);
// get raw zmq socket from specific SUB socket type
SOCKET_GET_RAW(socket_sub);

SOCKET_CONNECT(socket_sub);

SOCKET_BIND(socket_sub);

int socket_sub_subscribe(socket_sub_t* self, const char* topic)
{
    if (!self)
    {
        printf("E: socket_sub_subscribe on %s  failed socket is NULL: %s\n", topic, strerror(errno));
    }
    int rc = zmq_setsockopt(self->zmq_socket, ZMQ_SUBSCRIBE, topic, strlen(topic));
    if (rc == -1)
    {

        printf("E: socket_sub_subscribe on %s  failed: %s\n", topic, strerror(errno));
    }
    return rc;
}

int socket_sub_unsubscribe(socket_sub_t* self, const char* topic)
{
    if (!self)
    {
        printf("E: socket_sub_unsubscribe on %s  failed socket is NULL: %s\n", topic, strerror(errno));
    }
    int rc = zmq_setsockopt(self->zmq_socket, ZMQ_UNSUBSCRIBE, topic, strlen(topic));
    if (rc == -1)
    {

        printf("E: socket_sub_unsubscribe on %s  failed: %s\n", topic, strerror(errno));
    }
    return rc;
}

SOCKET_RECEIVE(socket_sub);

SOCKET_MESSAGE_RECEIVE_AS_STRING(socket_sub);

SOCKET_MESSAGE_RECEIVE(socket_sub);

SOCKET_SET_ID(socket_sub);

// --- PUB ---

// new and destroy socket of given type
IMPL_SOCKET_LIFECYCLE(socket_pub, ZMQ_PUB)
// get raw zmq socket from specific PUB socket type
SOCKET_GET_RAW(socket_pub);

SOCKET_BIND(socket_pub);

SOCKET_CONNECT(socket_pub);

SOCKET_SEND(socket_pub);

SOCKET_MESSAGE_SEND_AS_STRING(socket_pub);

SOCKET_MESSAGE_SEND(socket_pub);

SOCKET_SET_ID(socket_pub);

// --- REQ ---

// generate new/destroy for all socket types
IMPL_SOCKET_LIFECYCLE(socket_req, ZMQ_REQ)
// get raw zmq socket from specific REQ socket type
SOCKET_GET_RAW(socket_req);

SOCKET_CONNECT(socket_req);

SOCKET_SEND(socket_req);

SOCKET_MESSAGE_SEND_AS_STRING(socket_req);

SOCKET_MESSAGE_SEND(socket_req);

SOCKET_RECEIVE(socket_req);

SOCKET_MESSAGE_RECEIVE_AS_STRING(socket_req);

SOCKET_MESSAGE_RECEIVE(socket_req);

SOCKET_SET_ID(socket_req);

// --- REP ---

// generate new/destroy for all socket types
IMPL_SOCKET_LIFECYCLE(socket_rep, ZMQ_REP)
// get raw zmq socket from specific REP socket type
SOCKET_GET_RAW(socket_rep);

SOCKET_BIND(socket_rep);

SOCKET_RECEIVE(socket_rep);

SOCKET_MESSAGE_RECEIVE_AS_STRING(socket_rep);

SOCKET_MESSAGE_RECEIVE(socket_rep);

SOCKET_SEND(socket_rep);

SOCKET_MESSAGE_SEND_AS_STRING(socket_rep);

SOCKET_MESSAGE_SEND(socket_rep);

SOCKET_SET_ID(socket_rep);

// --- PAIR ---

// generate new/destroy for all socket types
IMPL_SOCKET_LIFECYCLE(socket_pair, ZMQ_PAIR)
// get raw zmq socket from specific PAIR socket type
SOCKET_GET_RAW(socket_pair);

SOCKET_BIND(socket_pair);

SOCKET_CONNECT(socket_pair);

SOCKET_SEND(socket_pair);

SOCKET_MESSAGE_RECEIVE_AS_STRING(socket_pair);

SOCKET_MESSAGE_RECEIVE(socket_pair);

SOCKET_RECEIVE(socket_pair);

SOCKET_MESSAGE_SEND_AS_STRING(socket_pair);

SOCKET_MESSAGE_SEND(socket_pair);

SOCKET_SET_ID(socket_pair);

// PUSH

// generate new/destroy for all socket types
IMPL_SOCKET_LIFECYCLE(socket_push, ZMQ_PUSH)
// get raw zmq socket from specific PUSH socket type
SOCKET_GET_RAW(socket_push);

SOCKET_BIND(socket_push);

SOCKET_CONNECT(socket_push);

SOCKET_SEND(socket_push);

SOCKET_MESSAGE_SEND_AS_STRING(socket_push);

SOCKET_MESSAGE_SEND(socket_push);

SOCKET_RECEIVE(socket_push);

SOCKET_MESSAGE_RECEIVE_AS_STRING(socket_push);

SOCKET_MESSAGE_RECEIVE(socket_push);

SOCKET_SET_ID(socket_push);

// --- PULL ---

// generate new/destroy for all socket types
IMPL_SOCKET_LIFECYCLE(socket_pull, ZMQ_PULL)
// get raw zmq socket from specific PULL socket type
SOCKET_GET_RAW(socket_pull);

SOCKET_BIND(socket_pull);

SOCKET_CONNECT(socket_pull);

SOCKET_SEND(socket_pull);

SOCKET_MESSAGE_SEND_AS_STRING(socket_pull);

SOCKET_MESSAGE_SEND(socket_pull);

SOCKET_RECEIVE(socket_pull);

SOCKET_MESSAGE_RECEIVE_AS_STRING(socket_pull);

SOCKET_MESSAGE_RECEIVE(socket_pull);

SOCKET_SET_ID(socket_pull);

// --- ROUTER ---

// generate new/destroy for all socket types
IMPL_SOCKET_LIFECYCLE(socket_router, ZMQ_ROUTER)
// get raw zmq socket from specific router socket type
SOCKET_GET_RAW(socket_router);

SOCKET_BIND(socket_router);

SOCKET_CONNECT(socket_router);

SOCKET_SEND(socket_router);

SOCKET_MESSAGE_SEND_AS_STRING(socket_router);

SOCKET_MESSAGE_SEND(socket_router);

SOCKET_RECEIVE(socket_router);

SOCKET_MESSAGE_RECEIVE_AS_STRING(socket_router);

SOCKET_MESSAGE_RECEIVE(socket_router);

SOCKET_SET_ID(socket_router);

// --- DEALER ---

// generate new/destroy for all socket types
IMPL_SOCKET_LIFECYCLE(socket_dealer, ZMQ_DEALER)
// get raw zmq socket from specific dealer socket type
SOCKET_GET_RAW(socket_dealer);

SOCKET_BIND(socket_dealer);

SOCKET_CONNECT(socket_dealer);

SOCKET_SEND(socket_dealer);

SOCKET_MESSAGE_SEND_AS_STRING(socket_dealer);

SOCKET_MESSAGE_SEND(socket_dealer);

SOCKET_RECEIVE(socket_dealer);

SOCKET_MESSAGE_RECEIVE_AS_STRING(socket_dealer);

SOCKET_MESSAGE_RECEIVE(socket_dealer);

SOCKET_SET_ID(socket_dealer);

void socket_destroy_helper(void* raw_socket)
{
    if (raw_socket)
    {
        int linger = 0;
        // Wymuś natychmiastowe porzucenie zbuforowanych pakietów
        zmq_setsockopt(raw_socket, ZMQ_LINGER, &linger, sizeof(linger));
        zmq_close(raw_socket);
    }
}

/**
 * @brief Receives a single ZeroMQ message frame and converts it to a null-terminated string.
 *
 * This function allocates memory for the resulting string using malloc().
 * The caller is strictly responsible for freeing the returned pointer using free()
 * to prevent memory leaks.
 *
 * @param[in]  socket   Pointer to the ZeroMQ socket.
 * @param[out] out_more Optional pointer to an integer. Set to 1 if there are more
 *                      frames to follow in a multipart message, or 0 otherwise.
 *                      Pass NULL if not needed.
 *
 * @return A dynamically allocated null-terminated string containing the frame data,
 *         or NULL if the reception failed.
 */
char* recv_frame_as_string(void* socket, int* out_more)
{
    zmq_msg_t msg;
    zmq_msg_init(&msg);

    if (zmq_msg_recv(&msg, socket, 0) == -1)
    {
        printf("E: recv_frame_as_string failed: %s\n", strerror(errno));
        zmq_msg_close(&msg);
        return NULL;
    }

    size_t size = zmq_msg_size(&msg);
    char*  str  = malloc(size + 1);
    memcpy(str, zmq_msg_data(&msg), size);
    str[size] = '\0';

    if (out_more)
    {
        *out_more = zmq_msg_more(&msg);
    }

    zmq_msg_close(&msg);
    return str;
}

/**
 * @brief Sends a null-terminated string as a ZeroMQ message frame.
 *
 * This function creates a ZeroMQ message from the provided string, sends it
 * through the specified socket, and automatically cleans up its internal message resources.
 *
 * @param[in] socket Pointer to the ZeroMQ socket.
 * @param[in] str    The null-terminated string to be sent. Must not be NULL.
 * @param[in] flags  ZeroMQ message flags (e.g., ZMQ_DONTWAIT, ZMQ_SNDMORE).
 *
 * @return The number of bytes sent on success, or -1 if the operation failed.
 */
int send_frame_string(void* socket, const char* str, int flags)
{
    zmq_msg_t msg;
    size_t    len = strlen(str);
    zmq_msg_init_size(&msg, len);
    memcpy(zmq_msg_data(&msg), str, len);

    int rc = zmq_msg_send(&msg, socket, flags);
    if (rc == -1)
    {
        printf("E: send_frame_string failed: %s\n", strerror(errno));
    }
    zmq_msg_close(&msg);
    return rc;
}

/**
 * @brief Receives a binary frame from a ZeroMQ socket.
 *
 * Allocates a memory buffer for the incoming message and copies its binary payload.
 *
 * @param socket Pointer to the ZeroMQ socket.
 * @param out_size Pointer to store the size of the received buffer in bytes (optional, can be NULL).
 * @param out_more Pointer to store the ZMQ_MORE flag state (optional, can be NULL).
 * @return Pointer to the allocated byte array, or NULL on failure.
 */
uint8_t* recv_frame_bytes(void* socket, size_t* out_size, int* out_more)
{
    zmq_msg_t msg;
    zmq_msg_init(&msg);

    if (zmq_msg_recv(&msg, socket, 0) == -1)
    {
        zmq_msg_close(&msg);
        if (out_size)
            *out_size = 0;
        return NULL;
    }

    size_t   size   = zmq_msg_size(&msg);
    uint8_t* buffer = (uint8_t*)malloc(size);
    if (!buffer)
    {
        zmq_msg_close(&msg);
        if (out_size)
            *out_size = 0;
        return NULL;
    }

    memcpy(buffer, zmq_msg_data(&msg), size);

    if (out_size)
    {
        *out_size = size;
    }

    if (out_more)
    {
        *out_more = zmq_msg_more(&msg);
    }

    zmq_msg_close(&msg);
    return buffer;
}

/**
 * @brief Sends a binary frame over a ZeroMQ socket.
 *
 * @param socket Pointer to the ZeroMQ socket.
 * @param data Pointer to the buffer containing binary data to send.
 * @param len Size of the binary data in bytes.
 * @param flags ZeroMQ send flags (e.g., ZMQ_SNDMORE or 0).
 * @return Number of bytes sent, or -1 on error.
 */
int send_frame_bytes(void* socket, const uint8_t* data, size_t len, int flags)
{
    zmq_msg_t msg;
    if (zmq_msg_init_size(&msg, len) != 0)
    {
        return -1;
    }

    memcpy(zmq_msg_data(&msg), data, len);

    int rc = zmq_msg_send(&msg, socket, flags);
    zmq_msg_close(&msg);
    return rc;
}
