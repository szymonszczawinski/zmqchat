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
