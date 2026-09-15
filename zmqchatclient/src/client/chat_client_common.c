#include "chat_client_common.h"

#include <stdarg.h>
#include <stdio.h>

void log_app_print(const char* format, ...)
{
    char    body[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(body, sizeof(body), format, args);
    va_end(args);

    char full_log[1100];
    snprintf(full_log, sizeof(full_log), "%s", body);
    printf("[LOG]%s", full_log);
}
