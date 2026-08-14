#ifndef ZQM_CLIENT_REQ_H
#define ZQM_CLIENT_REQ_H
#include <signal.h>

typedef struct
{
    void*                  context;
    char                   username[64];
    volatile sig_atomic_t* running;
} RequesterArgs;

void* requester_routine(void* arg);
#endif  // !ZQM_CLIENT_REQ_H
