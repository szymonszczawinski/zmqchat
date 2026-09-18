#include "chat_client_msg.h"
#include <zmq.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Creates and initializes an empty message structure.
 *
 * @return Pointer to allocated msg_t, or NULL on failure.
 */
msg_t* msg_new(void)
{
    msg_t* msg = (msg_t*)malloc(sizeof(msg_t));
    if (!msg)
        return NULL;
    msg->frames = NULL;
    msg->count  = 0;
    return msg;
}

/**
 * @brief Frees all allocated frames and the message structure itself.
 *
 * @param msg_ptr Pointer to the msg_t pointer (will be set to NULL).
 */
void msg_destroy(msg_t** msg_ptr)
{
    if (!msg_ptr || !*msg_ptr)
        return;

    msg_t* msg = *msg_ptr;
    for (size_t i = 0; i < msg->count; i++)
    {
        free(msg->frames[i].data);
    }
    free(msg->frames);
    free(msg);
    *msg_ptr = NULL;
}

/**
 * @brief Adds a new raw data frame to the message container.
 *
 * @param msg Pointer to the msg_t structure.
 * @param data Pointer to the payload bytes to copy.
 * @param size Size of the payload in bytes.
 * @return 0 on success, -1 on allocation error.
 */
int msg_add_frame(msg_t* msg, const uint8_t* data, size_t size)
{
    if (!msg)
        return -1;

    frame_t* new_frames = (frame_t*)realloc(msg->frames, sizeof(frame_t) * (msg->count + 1));
    if (!new_frames)
        return -1;

    msg->frames = new_frames;

    uint8_t* frame_data = (uint8_t*)malloc(size);
    if (!frame_data && size > 0)
        return -1;

    if (size > 0)
    {
        memcpy(frame_data, data, size);
    }

    msg->frames[msg->count].data = frame_data;
    msg->frames[msg->count].size = size;
    msg->count++;

    return 0;
}

/**
 * @brief Receives all frames of a multi-part message from a ZeroMQ socket.
 *
 * Loops until zmq_msg_more returns 0.
 *
 * @param socket Pointer to the ZeroMQ socket.
 * @return Pointer to the allocated msg_t containing all frames, or NULL on error.
 */

msg_t* msg_recv(void* socket)
{
    msg_t* msg = msg_new();
    if (!msg)
        return NULL;

    int more = 0;
    do
    {
        zmq_msg_t frame;
        zmq_msg_init(&frame);

        if (zmq_msg_recv(&frame, socket, 0) == -1)
        {
            zmq_msg_close(&frame);
            msg_destroy(&msg);
            return NULL;
        }

        size_t   size = zmq_msg_size(&frame);
        uint8_t* data = (uint8_t*)zmq_msg_data(&frame);

        if (msg_add_frame(msg, data, size) != 0)
        {
            zmq_msg_close(&frame);
            msg_destroy(&msg);
            return NULL;
        }

        more = zmq_msg_more(&frame);
        zmq_msg_close(&frame);

    } while (more);

    return msg;
}

/**
 * @brief Sends a multi-part message structure over a ZeroMQ socket.
 *
 * Automatically attaches ZMQ_SNDMORE flag for all frames except the last one.
 *
 * @param msg Pointer to the msg_t structure to send.
 * @param socket Pointer to the ZeroMQ socket.
 * @param flags Additional ZeroMQ flags (e.g. ZMQ_DONTWAIT).
 * @return 0 on success, -1 on failure.
 */
int msg_send(msg_t* msg, void* socket, int flags)
{
    if (!msg || msg->count == 0)
        return -1;

    for (size_t i = 0; i < msg->count; i++)
    {
        zmq_msg_t frame;
        size_t    len = msg->frames[i].size;

        if (zmq_msg_init_size(&frame, len) != 0)
        {
            return -1;
        }

        memcpy(zmq_msg_data(&frame), msg->frames[i].data, len);

        int send_flags = flags;
        if (i < msg->count - 1)
        {
            send_flags |= ZMQ_SNDMORE;
        }

        if (zmq_msg_send(&frame, socket, send_flags) == -1)
        {
            zmq_msg_close(&frame);
            return -1;
        }

        zmq_msg_close(&frame);
    }

    return 0;
}
