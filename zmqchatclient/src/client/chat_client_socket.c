#include "chat_client_socket.h"
#include <stdlib.h>
#include <string.h>

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

// MACRO for new/destroy for all socket types
#define IMPL_SOCKET_LIFECYCLE(type_name, zmq_type)                                      \
    type_name##_t* type_name##_new(void* zmq_context)                                   \
    {                                                                                   \
        if (!zmq_context)                                                               \
            return NULL;                                                                \
        type_name##_t* self = malloc(sizeof(type_name##_t));                            \
        if (!self)                                                                      \
            return NULL;                                                                \
        self->zmq_socket = zmq_socket(zmq_context, zmq_type);                           \
        if (!self->zmq_socket)                                                          \
        {                                                                               \
            free(self);                                                                 \
            return NULL;                                                                \
        }                                                                               \
        return self;                                                                    \
    }                                                                                   \
    void type_name##_destroy(type_name##_t** self_p)                                    \
    {                                                                                   \
        if (self_p && *self_p)                                                          \
        {                                                                               \
            int linger = 0;                                                             \
            zmq_setsockopt((*self_p)->zmq_socket, ZMQ_LINGER, &linger, sizeof(linger)); \
            zmq_close((*self_p)->zmq_socket);                                           \
            free(*self_p);                                                              \
            *self_p = NULL;                                                             \
        }                                                                               \
    }

// generate new/destroy for all socket types
IMPL_SOCKET_LIFECYCLE(socket_sub, ZMQ_SUB)
IMPL_SOCKET_LIFECYCLE(socket_pub, ZMQ_PUB)
IMPL_SOCKET_LIFECYCLE(socket_req, ZMQ_REQ)
IMPL_SOCKET_LIFECYCLE(socket_rep, ZMQ_REP)
IMPL_SOCKET_LIFECYCLE(socket_pair, ZMQ_PAIR)
IMPL_SOCKET_LIFECYCLE(socket_push, ZMQ_PUSH)
IMPL_SOCKET_LIFECYCLE(socket_pull, ZMQ_PULL)

// get raw zmq socket from specific SUB socket type
void* socket_sub_get_raw(socket_sub_t* self)
{
    return self ? self->zmq_socket : NULL;
}

// get raw zmq socket from specific PUB socket type
void* socket_pub_get_raw(socket_pub_t* self)
{
    return self ? self->zmq_socket : NULL;
}

// get raw zmq socket from specific REQ socket type
void* socket_req_get_raw(socket_req_t* self)
{
    return self ? self->zmq_socket : NULL;
}

// get raw zmq socket from specific REP socket type
void* socket_rep_get_raw(socket_rep_t* self)
{
    return self ? self->zmq_socket : NULL;
}

// get raw zmq socket from specific PAIR socket type
void* socket_pair_get_raw(socket_pair_t* self)
{
    return self ? self->zmq_socket : NULL;
}

// get raw zmq socket from specific PUSH socket type
void* socket_push_get_raw(socket_push_t* self)
{
    return self ? self->zmq_socket : NULL;
}

// get raw zmq socket from specific PULL socket type
void* socket_pull_get_raw(socket_pull_t* self)
{
    return self ? self->zmq_socket : NULL;
}

// --- SUB ---
int socket_sub_connect(socket_sub_t* sub, const char* ep)
{
    return sub ? zmq_connect(sub->zmq_socket, ep) : -1;
}

int socket_sub_bind(socket_sub_t* sub, const char* ep)
{
    return sub ? zmq_bind(sub->zmq_socket, ep) : -1;
}

int socket_sub_subscribe(socket_sub_t* sub, const char* topic)
{
    return sub ? zmq_setsockopt(sub->zmq_socket, ZMQ_SUBSCRIBE, topic, strlen(topic)) : -1;
}

int socket_sub_unsubscribe(socket_sub_t* sub, const char* topic)
{
    return sub ? zmq_setsockopt(sub->zmq_socket, ZMQ_UNSUBSCRIBE, topic, strlen(topic)) : -1;
}

int socket_sub_recv(socket_sub_t* sub, void* buf, size_t len, int flags)
{
    return sub ? zmq_recv(sub->zmq_socket, buf, len, flags) : -1;
}

// --- PUB ---
int socket_pub_bind(socket_pub_t* pub, const char* ep)
{
    return pub ? zmq_bind(pub->zmq_socket, ep) : -1;
}

int socket_pub_connect(socket_pub_t* pub, const char* ep)
{
    return pub ? zmq_connect(pub->zmq_socket, ep) : -1;
}

int socket_pub_send(socket_pub_t* pub, const void* buf, size_t len, int flags)
{
    return pub ? zmq_send(pub->zmq_socket, buf, len, flags) : -1;
}

// --- REQ ---
int socket_req_connect(socket_req_t* req, const char* ep)
{
    return req ? zmq_connect(req->zmq_socket, ep) : -1;
}

int socket_req_send(socket_req_t* req, const void* buf, size_t len, int flags)
{
    return req ? zmq_send(req->zmq_socket, buf, len, flags) : -1;
}

int socket_req_recv(socket_req_t* req, void* buf, size_t len, int flags)
{
    return req ? zmq_recv(req->zmq_socket, buf, len, flags) : -1;
}

// --- REP ---
int socket_rep_bind(socket_rep_t* rep, const char* ep)
{
    return rep ? zmq_bind(rep->zmq_socket, ep) : -1;
}

int socket_rep_recv(socket_rep_t* rep, void* buf, size_t len, int flags)
{
    return rep ? zmq_recv(rep->zmq_socket, buf, len, flags) : -1;
}

int socket_rep_send(socket_rep_t* rep, const void* buf, size_t len, int flags)
{
    return rep ? zmq_send(rep->zmq_socket, buf, len, flags) : -1;
}

// --- PAIR ---
int socket_pair_bind(socket_pair_t* pair, const char* ep)
{
    return pair ? zmq_bind(pair->zmq_socket, ep) : -1;
}

int socket_pair_connect(socket_pair_t* pair, const char* ep)
{
    return pair ? zmq_connect(pair->zmq_socket, ep) : -1;
}

int socket_pair_send(socket_pair_t* pair, const void* buf, size_t len, int flags)
{
    return pair ? zmq_send(pair->zmq_socket, buf, len, flags) : -1;
}

int socket_pair_recv(socket_pair_t* pair, void* buf, size_t len, int flags)
{
    return pair ? zmq_recv(pair->zmq_socket, buf, len, flags) : -1;
}

// PUSH

int socket_push_bind(socket_push_t* push, const char* ep)
{
    return push ? zmq_bind(push->zmq_socket, ep) : -1;
}

int socket_push_connect(socket_push_t* push, const char* ep)
{
    return push ? zmq_connect(push->zmq_socket, ep) : -1;
}

int socket_push_send(socket_push_t* push, const void* buf, size_t len, int flags)
{
    return push ? zmq_send(push->zmq_socket, buf, len, flags) : -1;
}

int socket_push_recv(socket_push_t* push, void* buf, size_t len, int flags)
{
    return push ? zmq_recv(push->zmq_socket, buf, len, flags) : -1;
}

// PULL

int socket_pull_bind(socket_pull_t* pull, const char* ep)
{
    return pull ? zmq_bind(pull->zmq_socket, ep) : -1;
}

int socket_pull_connect(socket_pull_t* pull, const char* ep)
{
    return pull ? zmq_connect(pull->zmq_socket, ep) : -1;
}

int socket_pull_send(socket_pull_t* pull, const void* buf, size_t len, int flags)
{
    return pull ? zmq_send(pull->zmq_socket, buf, len, flags) : -1;
}

int socket_pull_recv(socket_pull_t* pull, void* buf, size_t len, int flags)
{
    return pull ? zmq_recv(pull->zmq_socket, buf, len, flags) : -1;
}

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
