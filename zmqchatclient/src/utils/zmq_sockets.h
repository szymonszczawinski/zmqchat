#ifndef ZMQ_SOCKETS_H
#define ZMQ_SOCKETS_H
#include "zmq_messages.h"
#include <zmq.h>

// types

typedef struct socket_sub    socket_sub_t;
typedef struct socket_pub    socket_pub_t;
typedef struct socket_req    socket_req_t;
typedef struct socket_rep    socket_rep_t;
typedef struct socket_pair   socket_pair_t;
typedef struct socket_push   socket_push_t;
typedef struct socket_pull   socket_pull_t;
typedef struct socket_router socket_router_t;
typedef struct socket_dealer socket_dealer_t;

// SUB

socket_sub_t*  socket_sub_new(void* zmq_context);
void           socket_sub_destroy(socket_sub_t** self_p);
void*          socket_sub_get_raw(socket_sub_t* self);
int            socket_sub_connect(socket_sub_t* self, const char* endpoint);
int            socket_sub_bind(socket_sub_t* self, const char* endpoint);
int            socket_sub_subscribe(socket_sub_t* self, const char* topic);
int            socket_sub_unsubscribe(socket_sub_t* self, const char* topic);
int            socket_sub_recv(socket_sub_t* self, void* buf, size_t len, int flags);
char*          socket_sub_recv_string(socket_sub_t* self, int flags);
zmq_message_t* socket_sub_message_recv(socket_sub_t* self);
int            socket_sub_set_id(socket_sub_t* self, char* identity);

// PUB

socket_pub_t* socket_pub_new(void* zmq_context);
void          socket_pub_destroy(socket_pub_t** self_p);
void*         socket_pub_get_raw(socket_pub_t* self);
int           socket_pub_bind(socket_pub_t* self, const char* endpoint);
int           socket_pub_connect(socket_pub_t* self, const char* ep);
int           socket_pub_send(socket_pub_t* self, const void* buf, size_t len, int flags);
int           socket_pub_send_string(socket_pub_t* self, const char* str, int flags);
int           socket_pub_message_send(socket_pub_t* self, zmq_message_t* msg, int flags);
int           socket_pub_set_id(socket_pub_t* self, char* identity);

// REQ

socket_req_t*  socket_req_new(void* zmq_context);
void           socket_req_destroy(socket_req_t** self_p);
void*          socket_req_get_raw(socket_req_t* self);
int            socket_req_connect(socket_req_t* self, const char* endpoint);
int            socket_req_send(socket_req_t* self, const void* buf, size_t len, int flags);
int            socket_req_send_string(socket_req_t* self, const char* str, int flags);
int            socket_req_message_send(socket_req_t* self, zmq_message_t* msg, int flags);
int            socket_req_recv(socket_req_t* self, void* buf, size_t len, int flags);
char*          socket_req_recv_string(socket_req_t* self, int flags);
zmq_message_t* socket_req_message_recv(socket_req_t* self);
int            socket_req_set_id(socket_req_t* self, char* identity);

// REP

socket_rep_t*  socket_rep_new(void* zmq_context);
void           socket_rep_destroy(socket_rep_t** self_p);
void*          socket_rep_get_raw(socket_rep_t* self);
int            socket_rep_bind(socket_rep_t* self, const char* endpoint);
int            socket_rep_recv(socket_rep_t* self, void* buf, size_t len, int flags);
char*          socket_rep_recv_string(socket_rep_t* self, int flags);
zmq_message_t* socket_rep_message_recv(socket_rep_t* self);
int            socket_rep_send(socket_rep_t* self, const void* buf, size_t len, int flags);
int            socket_rep_send_string(socket_rep_t* self, const char* str, int flags);
int            socket_rep_message_send(socket_rep_t* self, zmq_message_t* msg, int flags);
int            socket_rep_set_id(socket_rep_t* self, char* identity);

// PAIR (Peer-to-Peer 1:1)

socket_pair_t* socket_pair_new(void* zmq_context);
void           socket_pair_destroy(socket_pair_t** self_p);
void*          socket_pair_get_raw(socket_pair_t* self);
int            socket_pair_bind(socket_pair_t* self, const char* endpoint);
int            socket_pair_connect(socket_pair_t* self, const char* endpoint);
int            socket_pair_send(socket_pair_t* self, const void* buf, size_t len, int flags);
int            socket_pair_send_string(socket_pair_t* self, const char* str, int flags);
int            socket_pair_message_send(socket_pair_t* self, zmq_message_t* msg, int flags);
int            socket_pair_recv(socket_pair_t* self, void* buf, size_t len, int flags);
char*          socket_pair_recv_string(socket_pair_t* self, int flags);
zmq_message_t* socket_pair_message_recv(socket_pair_t* self);
int            socket_pair_set_id(socket_pair_t* self, char* identity);

// PUSH

socket_push_t* socket_push_new(void* zmq_context);
void           socket_push_destroy(socket_push_t** self_p);
void*          socket_push_get_raw(socket_push_t* self);
int            socket_push_bind(socket_push_t* self, const char* endpoint);
int            socket_push_connect(socket_push_t* push, const char* endpoint);
int            socket_push_send(socket_push_t* push, const void* buf, size_t len, int flags);
int            socket_push_send_string(socket_push_t* self, const char* str, int flags);
int            socket_push_message_send(socket_push_t* self, zmq_message_t* msg, int flags);
int            socket_push_recv(socket_push_t* self, void* buf, size_t len, int flags);
char*          socket_push_recv_string(socket_push_t* self, int flags);
zmq_message_t* socket_push_message_recv(socket_push_t* self);
int            socket_push_set_id(socket_push_t* self, char* identity);

// PULL

socket_pull_t* socket_pull_new(void* zmq_context);
void           socket_pull_destroy(socket_pull_t** self_p);
void*          socket_pull_get_raw(socket_pull_t* self);
int            socket_pull_bind(socket_pull_t* self, const char* endpoint);
int            socket_pull_connect(socket_pull_t* self, const char* endpoint);
int            socket_pull_send(socket_pull_t* self, const void* buf, size_t len, int flags);
int            socket_pull_send_string(socket_pull_t* self, const char* str, int flags);
int            socket_pull_message_send(socket_pull_t* self, zmq_message_t* msg, int flags);
int            socket_pull_recv(socket_pull_t* self, void* buf, size_t len, int flags);
char*          socket_pull_recv_string(socket_pull_t* self, int flags);
zmq_message_t* socket_pull_message_recv(socket_pull_t* self);
int            socket_pull_set_id(socket_pull_t* self, char* identity);

// ROUTER

socket_router_t* socket_router_new(void* zmq_context);
void             socket_router_destroy(socket_router_t** self_p);
void*            socket_router_get_raw(socket_router_t* self);
int              socket_router_bind(socket_router_t* self, const char* endpoint);
int              socket_router_connect(socket_router_t* self, const char* endpoint);
int              socket_router_send(socket_router_t* self, const void* buf, size_t len, int flags);
int              socket_router_send_string(socket_router_t* self, const char* str, int flags);
int              socket_router_message_send(socket_router_t* self, zmq_message_t* msg, int flags);
int              socket_router_recv(socket_router_t* self, void* buf, size_t len, int flags);
char*            socket_router_recv_string(socket_router_t* self, int flags);
zmq_message_t*   socket_router_message_recv(socket_router_t* self);
int              socket_router_set_id(socket_router_t* self, char* identity);
// DEALER

socket_dealer_t* socket_dealer_new(void* zmq_context);
void             socket_dealer_destroy(socket_dealer_t** self_p);
void*            socket_dealer_get_raw(socket_dealer_t* self);
int              socket_dealer_bind(socket_dealer_t* self, const char* endpoint);
int              socket_dealer_connect(socket_dealer_t* self, const char* endpoint);
int              socket_dealer_send(socket_dealer_t* self, const void* buf, size_t len, int flags);
int              socket_dealer_send_string(socket_dealer_t* self, const char* str, int flags);
int              socket_dealer_message_send(socket_dealer_t* self, zmq_message_t* msg, int flags);
int              socket_dealer_recv(socket_dealer_t* self, void* buf, size_t len, int flags);
char*            socket_dealer_recv_string(socket_dealer_t* self, int flags);
zmq_message_t*   socket_dealer_message_recv(socket_dealer_t* self);
int              socket_dealer_set_id(socket_dealer_t* self, char* identity);

// UTILS

char*    recv_frame_as_string(void* socket, int* out_more);
int      send_frame_string(void* socket, const char* str, int flags);
uint8_t* recv_frame_bytes(void* socket, size_t* out_size, int* out_more);
int      send_frame_bytes(void* socket, const uint8_t* data, size_t len, int flags);

#endif  // !ZMQ_SOCKETS_H
