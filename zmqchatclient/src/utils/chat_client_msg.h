#ifndef CHAT_CLIENT_MSG_H
#define CHAT_CLIENT_MSG_H()
#include <zmq.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Represents a single frame in a multi-part message.
 */
typedef struct
{
    uint8_t* data; /**< Pointer to raw binary payload */
    size_t   size; /**< Size of the payload in bytes */
} frame_t;

/**
 * @brief Represents a multi-part message consisting of several frames.
 */
typedef struct
{
    frame_t* frames; /**< Array of frames */
    size_t   count;  /**< Total number of frames in the message */
} msg_t;

msg_t* msg_new(void);
void   msg_destroy(msg_t** msg_ptr);
int    msg_add_frame(msg_t* msg, const uint8_t* data, size_t size);
msg_t* msg_recv(void* socket);
int    msg_send(msg_t* msg, void* socket, int flags);
#endif  // !CHAT_CLIENT_MSG_H
