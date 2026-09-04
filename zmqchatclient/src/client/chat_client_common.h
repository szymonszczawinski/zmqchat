#ifndef ZMQ_CLIENT_COMMON_H
#define ZMQ_CLIENT_COMMON_H

static const char* INPROC_SHUTDOWN_ADDR                = "inproc://shutdown";
static const char* CHAT_SERVER_SUB_ADDRESS             = "tcp://localhost:5556";
static const char* CHAT_SERVER_REQ_ADDRESS             = "tcp://localhost:5555";
static const char* CONTROLLER_RECEIVER_COMMAND_ADDRESS = "inproc://controller-receiver";
static const char* CONROLLER_UI_COMMAND_ADDRESS        = "inproc://controller-ui";
static const char* CONTROLLER_UI_NOTIFICATION_ADDRESS  = "inproc://ui-notifications";

#endif  // !ZMQ_CLIENT_COMMON_H
