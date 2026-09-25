#ifndef ZMQ_MESSAGES_H
#define ZMQ_MESSAGES_H
#include <zmq.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct zmq_message_frame zmq_message_frame_t;

typedef struct zmq_message zmq_message_t;

zmq_message_t* message_new(void);
void           message_destroy(zmq_message_t** msg_ptr);
int            message_add_frame(zmq_message_t* msg, const uint8_t* data, size_t size);
zmq_message_t* message_recv(void* socket);
int            message_send(zmq_message_t* msg, void* socket, int flags);
#endif  // !ZMQ_MESSAGES_H
