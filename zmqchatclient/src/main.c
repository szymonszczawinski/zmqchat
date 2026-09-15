// #include "client/zmq_client.h"
#include "client/chat_client.h"
#include <stdio.h>
#include <zmq.h>

int main(int argc, char** argv)
{
    int mode = -1;
    printf("----- ZMQ CHAT CLIENT :: MAIN -----\n");
    int major, minor, patch;
    zmq_version(&major, &minor, &patch);
    printf("Current 0MQ version is %d.%d.%d\n", major, minor, patch);
    zmq_client_run();
}
