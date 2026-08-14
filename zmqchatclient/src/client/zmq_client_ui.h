#ifndef ZMQ_CLIENT_UI_H
#define ZMQ_CLIENT_UI_H
#include <signal.h>

typedef struct
{
    void*                  context;
    char                   username[64];
    volatile sig_atomic_t* running;
} UiArgs;

void* ui_routine(void* arg);
#endif  // !ZMQ_CLIENT_UI_H
