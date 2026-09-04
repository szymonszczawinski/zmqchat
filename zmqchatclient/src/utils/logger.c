#include "logger.h"
#include <stdarg.h>
#include <stdio.h>

static const char* levels[] = { "DEBUG", "INFO", "WARN", "ERROR" };

// 1. The Core Engine
// It now accepts the file name and line number as arguments
void logger_log_internal(LogLevel level, const char* file, int line, const char* message, ...)
{
    // Print metadata: [PREFIX] [file.c:line]
    printf("[%s] [%s:%d] ", levels[level], file, line);

    // Print the actual formatted message
    va_list args;
    va_start(args, message);
    (void)vprintf(message, args);
    va_end(args);

    printf("\n");
}

void logger_log_internal_ext(LogLevel level, const char* file, int line, const char* prefix, int count, ...)
{
    FILE* stream = (level >= LOG_LEVEL_WARN) ? stderr : stdout;

    fprintf(stream, "[%s] [%s:%d] %s -> ", levels[level], file, line, prefix);

    va_list ap;
    va_start(ap, count);
    for (int i = 0; i < count; i++)
    {
        LogParam p = va_arg(ap, LogParam);
        fprintf(stream, "{%s: ", p.name);
        switch (p.type)
        {
        case TYPE_INT:
            fprintf(stream, "%d", p.value.i);
            break;
        case TYPE_FLOAT:
            fprintf(stream, "%.2f", p.value.f);
            break;
        case TYPE_STRING:
            fprintf(stream, "%s", p.value.s);
            break;
        }
        fprintf(stream, "} ");
    }
    fprintf(stream, "\n");
    va_end(ap);
}
