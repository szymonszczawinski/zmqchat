#ifndef ZMQ_CLIENT_SUB_H
#define ZMQ_CLIENT_SUB_H

#include <signal.h>

typedef struct
{
    void*                  context;
    char                   username[64];
    volatile sig_atomic_t* running;
} SubscriberArgs;

void* subscriber_routine(void* arg);
#endif  // !ZMQ_CLIENT_SUB_H
