#include "chat_client.h"
#include "chat_client_controller.h"
#include "chat_client_receiver.h"
#include "chat_client_ui.h"
#include "chat_client_common.h"
#include "chat_client_socket.h"
#include "utils/signal_handler.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <zmq.h>

int zmq_client_run(void)
{
    // 1. init signal handler
    int sig_fd = -1;
    if (setup_signal_handler(&sig_fd) != 0)
    {
        return -1;
    }
    char username[64];
    printf("enter login: ");
    if (scanf("%63s", username) != 1)
        return 1;

    void* zmqcontext = zmq_ctx_new();
    assert(zmqcontext);
    // create PUB socket to send KILL signal via INPROC
    // void* socket_pub_shutdown = zmq_socket(zmqcontext, ZMQ_PUB);
    socket_pub_t* socket_pub_shutdown = socket_pub_new(zmqcontext);
    socket_pub_bind(socket_pub_shutdown, INPROC_SHUTDOWN_ADDR);

    pthread_t     thread_receiver;
    ReceiverArgs* args_receiver = malloc(sizeof(ReceiverArgs));
    args_receiver->context      = zmqcontext;
    strncpy(args_receiver->username, username, sizeof(args_receiver->username));
    pthread_create(&thread_receiver, NULL, routine_chat_receiver, args_receiver);

    pthread_t       thread_controller;
    ControllerArgs* args_controller = malloc(sizeof(ControllerArgs));
    args_controller->context        = zmqcontext;
    strncpy(args_controller->username, username, sizeof(args_controller->username));
    pthread_create(&thread_controller, NULL, routine_chat_controller, args_controller);

    pthread_t thread_ui;
    UiArgs*   args_ui = malloc(sizeof(UiArgs));
    args_ui->context  = zmqcontext;
    strncpy(args_ui->username, username, sizeof(args_ui->username));
    pthread_create(&thread_ui, NULL, routine_chat_ui, args_controller);

    // main thread blocks on sygnal Ctrl+C
    zmq_pollitem_t items[] = { { NULL, sig_fd, ZMQ_POLLIN, 0 } };

    printf("[MAIN] app running. press Ctrl+C to stop...\n");
    zmq_poll(items, 1, -1);  // sleep with 0% CPU until Ctrl+C

    printf("\n[MAIN] Ctrl+C! sending KILL via INPROC...\n");

    // sending KILL to all threads
    socket_pub_send(socket_pub_shutdown, "KILL", 4, 0);
    printf("[ZMQClient] context closed...\n");
    pthread_join(thread_receiver, NULL);
    pthread_join(thread_controller, NULL);
    pthread_join(thread_ui, NULL);

    printf("[MAIN] threads finished. Clean ZMQ context...\n");

    // close control socket and close zmq context
    socket_pub_destroy(&socket_pub_shutdown);
    zmq_ctx_destroy(zmqcontext);

    free(args_controller);
    free(args_receiver);
    free(args_ui);
    cleanup_signal_handler(sig_fd);
    return EXIT_SUCCESS;
}
