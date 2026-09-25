#ifndef ZMQ_UTILS_H
#define ZMQ_UTILS_H
#define _GNU_SOURCE

#include <time.h>
#include <sys/time.h>
#include <stdint.h>
#define randof(num) (int)((float)(num) * random() / (RAND_MAX + 1.0))
typedef void* (*thread_start_routine)(void*);
static const char* INPROC_SHUTDOWN_ADDR = "inproc://shutdown";

typedef struct zmq_thread_args
{
    void* context;
    char* id;
    char* topic_name;
    char* thread_name;
} zmq_thread_args_t;

typedef struct simple_thread_args
{
    void* zmq_context;
    char* thread_name;
} simple_thread_args_t;

zmq_thread_args_t* zmq_thread_args_create(void* zmqcontext, char* id, char* topic_name, char* thread_name);

void delay_ms(long milliseconds);

char* queue_pop(char* queue[], int* capacity);
void  queue_push(char* queue[], int* capacity, char* element, int max_capacity);
void  queue_cleanup(char* queue[], int capacity);
int   queue_size(char* queue[]);
bool  queue_is_empty(char* queue[]);

//  Return current system clock as milliseconds
static int64_t s_clock(void)
{
#if (defined(WIN32))
    SYSTEMTIME st;
    GetSystemTime(&st);
    return (int64_t)st.wSecond * 1000 + st.wMilliseconds;
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)(tv.tv_sec * 1000 + tv.tv_usec / 1000);
#endif
}

#if (defined(WIN32))
#define srandom srand
#define random  rand
#endif

#endif  // !ZMQ_UTILS_H
