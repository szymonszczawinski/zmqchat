#ifndef CHAT_CLIENT_SOCKET_H
#define CHAT_CLIENT_SOCKET_H
#include <zmq.h>

// types

typedef struct socket_sub  socket_sub_t;
typedef struct socket_pub  socket_pub_t;
typedef struct socket_req  socket_req_t;
typedef struct socket_rep  socket_rep_t;
typedef struct socket_pair socket_pair_t;

// SUB

socket_sub_t* socket_sub_new(void* zmq_context);
void          socket_sub_destroy(socket_sub_t** self_p);
int           socket_sub_connect(socket_sub_t* sub, const char* endpoint);
int           socket_sub_subscribe(socket_sub_t* sub, const char* topic);
int           socket_sub_recv(socket_sub_t* sub, void* buf, size_t len, int flags);

// PUB

socket_pub_t* socket_pub_new(void* zmq_context);
void          socket_pub_destroy(socket_pub_t** self_p);
int           socket_pub_bind(socket_pub_t* pub, const char* endpoint);
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
#endif  // !CHAT_CLIENT_SOCKET_H
