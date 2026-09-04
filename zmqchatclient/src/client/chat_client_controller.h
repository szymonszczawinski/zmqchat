#ifndef ZMQ_CLIENT_REQ_H
#define ZMQ_CLIENT_REQ_H

typedef struct
{
    void* context;
    char  username[64];
} ControllerArgs;

void* routine_chat_controller(void* arg);
#endif  // !ZMQ_CLIENT_REQ_H
