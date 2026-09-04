#ifndef ZMQ_CLIENT_SUB_H
#define ZMQ_CLIENT_SUB_H

typedef struct
{
    void* context;
    char  username[64];
} ReceiverArgs;

void* routine_chat_receiver(void* arg);
#endif  // !ZMQ_CLIENT_SUB_H
