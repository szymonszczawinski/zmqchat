#ifndef LOGGER_H
#define LOGGER_H

#include <stdarg.h>

// --- Definitions ---

typedef enum
{
    TYPE_INT,
    TYPE_FLOAT,
    TYPE_STRING
} LogParamType;

typedef enum
{
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEl_ERROR
} LogLevel;

typedef struct
{
    LogParamType type;
    const char*  name;

    union
    {
        int         i;
        float       f;
        const char* s;
    } value;
} LogParam;

// Helper Macros for inline Param creation
#define L_INT(n, v)                  \
    (LogParam)                       \
    {                                \
        TYPE_INT, n, .value.i = (v), \
    }
#define L_FLT(n, v)                    \
    (LogParam)                         \
    {                                  \
        TYPE_FLOAT, n, .value.f = (v), \
    }
#define L_STR(n, v)                     \
    (LogParam)                          \
    {                                   \
        TYPE_STRING, n, .value.s = (v), \
    }

void logger_log_internal(LogLevel level, const char* file, int line, const char* message, ...);
void logger_log_internal_ext(LogLevel level, const char* file, int line, const char* message, int count, ...);
// --- The Public Macros ---

#define log_debug(message, ...) logger_log_internal(LOG_LEVEL_DEBUG, __FILE__, __LINE__, message, ##__VA_ARGS__)
#define log_debug_ext(message, n, ...) \
    logger_log_internal_ext(LOG_LEVEL_DEBUG, __FILE__, __LINE__, message, n, __VA_ARGS__)
#define log_info(message, ...) logger_log_internal(LOG_LEVEL_INFO, __FILE__, __LINE__, message, ##__VA_ARGS__)
#define log_info_ext(message, n, ...) \
    logger_log_internal_ext(LOG_LEVEL_INFO, __FILE__, __LINE__, message, n, __VA_ARGS__)
#define log_warn(message, ...) logger_log_internal(LOG_LEVEL_WARN, __FILE__, __LINE__, message, ##__VA_ARGS__)
#define log_warn_ext(message, n, ...) \
    logger_log_internal_ext(LOG_LEVEL_WARN, __FILE__, __LINE__, message, n, __VA_ARGS__)
#define log_error(message, ...) logger_log_internal(LOG_LEVEL_ERROR, __FILE__, __LINE__, message, ##__VA_ARGS__)
#define log_error_ext(message, n, ...) \
    logger_log_internal_ext(LOG_LEVEL_ERROR, __FILE__, __LINE__, message, n, __VA_ARGS__)

#endif  // LOGGER_H
