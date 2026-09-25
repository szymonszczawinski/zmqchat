#define _POSIX_C_SOURCE 199309L
#include "zmq_utils.h"
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <zmq.h>

void delay_ms(long milliseconds)
{
    struct timespec ts;
    ts.tv_sec  = milliseconds / 1000;
    ts.tv_nsec = (milliseconds % 1000) * 1000000;
    nanosleep(&ts, NULL);
}

/**
 * @brief Allocate and return zmq_thread_args on heap. Need to dealocate manually.
 *
 * @param zmqcontext zmq context to create socket
 * @param id generic identifier (optional)
 * @param topic_name name of the topic to publish
 * @param thread_name name of the thread
 * @return pointer to allocated args
 */
zmq_thread_args_t* zmq_thread_args_create(void* zmqcontext, char* id, char* topic_name, char* thread_name)
{

    zmq_thread_args_t* args = malloc(sizeof(zmq_thread_args_t));
    args->context           = zmqcontext;
    args->id                = id;
    args->topic_name        = topic_name;
    args->thread_name       = thread_name;
    return args;
}

/**
 * @brief pops next element from queue and reduce capacity and nullify freed position
 *
 * @param queue array of elements
 * @param capacity current capacity
 * @return selected element
 */
char* queue_pop(char* queue[], int* capacity)
{
    if (*capacity <= 0)
    {
        return NULL;
    }

    char* element = queue[0];
    for (int i = 0; i < *capacity - 1; i++)
    {
        queue[i] = queue[i + 1];
    }
    (*capacity)--;
    queue[*capacity] = NULL;  // nullify freed position
    return element;
}

/**
 * @brief push new element to end of the queue and increase capacity
 *
 * @param queue array of elements
 * @param capacity current capacity
 * @param element entry to add
 * @param max_capacity maximum possible number of elements
 */
void queue_push(char* queue[], int* capacity, char* element, int max_capacity)
{
    if (*capacity < max_capacity)
    {
        queue[*capacity] = element;
        (*capacity)++;
    }
    else
    {
        fprintf(stderr, "ERR: Queue full, dropping element %s\n", element);
        free(element);
    }
}

/**
 * @brief clean whole elements queue and nullify removed entries
 *
 * @param queue array of elements
 * @param capacity current capacity
 */
void queue_cleanup(char* queue[], int capacity)
{
    for (int i = 0; i < capacity; i++)
    {
        if (queue[i] != NULL)
        {
            free(queue[i]);
            queue[i] = NULL;
        }
    }
}

int queue_size(char* queue[])
{
    int count = 0;
    // Loop until we hit the NULL sentinel
    while (queue[count] != NULL)
    {
        count++;
    }
    return count;
}

bool queue_is_empty(char* queue[])
{
    return queue_size(queue) == 0;
}
