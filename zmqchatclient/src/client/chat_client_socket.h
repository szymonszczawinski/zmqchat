#ifndef CHAT_CLIENT_SOCKET_H
#define CHAT_CLIENT_SOCKET_H
#include <zmq.h>

// types

typedef struct socket_sub  socket_sub_t;
typedef struct socket_pub  socket_pub_t;
typedef struct socket_req  socket_req_t;
typedef struct socket_rep  socket_rep_t;
typedef struct socket_pair socket_pair_t;
typedef struct socket_push socket_push_t;
typedef struct socket_pull socket_pull_t;

void* socket_sub_get_raw(socket_sub_t* self);
void* socket_pub_get_raw(socket_pub_t* self);
void* socket_req_get_raw(socket_req_t* self);
void* socket_rep_get_raw(socket_rep_t* self);
void* socket_pair_get_raw(socket_pair_t* self);
void* socket_push_get_raw(socket_push_t* self);
void* socket_pull_get_raw(socket_pull_t* self);
// SUB

socket_sub_t* socket_sub_new(void* zmq_context);
void          socket_sub_destroy(socket_sub_t** self_p);
int           socket_sub_connect(socket_sub_t* sub, const char* endpoint);
int           socket_sub_bind(socket_sub_t* sub, const char* endpoint);
int           socket_sub_subscribe(socket_sub_t* sub, const char* topic);
int           socket_sub_unsubscribe(socket_sub_t* sub, const char* topic);
int           socket_sub_recv(socket_sub_t* sub, void* buf, size_t len, int flags);

// PUB

socket_pub_t* socket_pub_new(void* zmq_context);
void          socket_pub_destroy(socket_pub_t** self_p);
int           socket_pub_bind(socket_pub_t* pub, const char* endpoint);
int           socket_pub_connect(socket_pub_t* pub, const char* ep);
int           socket_pub_send(socket_pub_t* pub, const void* buf, size_t len, int flags);

// REQ
socket_req_t* socket_req_new(void* zmq_context);
void          socket_req_destroy(socket_req_t** self_p);
int           socket_req_connect(socket_req_t* req, const char* endpoint);
int           socket_req_send(socket_req_t* req, const void* buf, size_t len, int flags);
int           socket_req_recv(socket_req_t* req, void* buf, size_t len, int flags);

// REP

socket_rep_t* socket_rep_new(void* zmq_context);
void          socket_rep_destroy(socket_rep_t** self_p);
int           socket_rep_bind(socket_rep_t* rep, const char* endpoint);
int           socket_rep_recv(socket_rep_t* rep, void* buf, size_t len, int flags);
int           socket_rep_send(socket_rep_t* rep, const void* buf, size_t len, int flags);

// PAIR (Peer-to-Peer 1:1)

socket_pair_t* socket_pair_new(void* zmq_context);
void           socket_pair_destroy(socket_pair_t** self_p);
int            socket_pair_bind(socket_pair_t* pair, const char* endpoint);
int            socket_pair_connect(socket_pair_t* pair, const char* endpoint);
int            socket_pair_send(socket_pair_t* pair, const void* buf, size_t len, int flags);
int            socket_pair_recv(socket_pair_t* pair, void* buf, size_t len, int flags);

// PUSH

socket_push_t* socket_push_new(void* zmq_context);
void           socket_push_destroy(socket_push_t** self_p);
int            socket_push_bind(socket_push_t* push, const char* endpoint);
int            socket_push_connect(socket_push_t* push, const char* endpoint);
int            socket_push_send(socket_push_t* push, const void* buf, size_t len, int flags);
int            socket_push_recv(socket_push_t* push, void* buf, size_t len, int flags);

// PULL

socket_pull_t* socket_pull_new(void* zmq_context);
void           socket_pull_destroy(socket_pull_t** self_p);
int            socket_pull_bind(socket_pull_t* pull, const char* endpoint);
int            socket_pull_connect(socket_pull_t* pull, const char* endpoint);
int            socket_pull_send(socket_pull_t* pull, const void* buf, size_t len, int flags);
int            socket_pull_recv(socket_pull_t* pull, void* buf, size_t len, int flags);

#endif  // !CHAT_CLIENT_SOCKET_H
