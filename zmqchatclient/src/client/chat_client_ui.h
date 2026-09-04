#ifndef ZMQ_CLIENT_UI_H
#define ZMQ_CLIENT_UI_H

typedef struct
{
    void* context;
    char  username[64];
} UiArgs;

void* routine_chat_ui(void* arg);
#endif  // !ZMQ_CLIENT_UI_H
