#ifndef ZMQ_CLIENT_COMMON_H
#define ZMQ_CLIENT_COMMON_H

// adrresses

static const char* INPROC_SHUTDOWN_ADDR                = "inproc://shutdown";
static const char* CHAT_SERVER_SUB_ADDRESS             = "tcp://localhost:5556";
static const char* CHAT_SERVER_REQ_ADDRESS             = "tcp://localhost:5555";
static const char* CONTROLLER_RECEIVER_COMMAND_ADDRESS = "inproc://controller-receiver";
static const char* CONROLLER_UI_COMMAND_ADDRESS        = "inproc://controller-ui";
static const char* CONTROLLER_UI_NOTIFICATION_ADDRESS  = "inproc://ui-notifications";

// commands

static const char* CHAT_COMMAND_QUIT  = "/quit";
static const char* CHAT_COMMAND_ROOMS = "/rooms";  //                     - lista pokojów\n");
static const char* CHAT_COMMAND_JOIN  = "/join";   // <nazwa_pokoju>       - dołącza do pokoju\n");
static const char* CHAT_COMMAND_LEAVE = "/leave";  // <nazwa_pokoju>      - opuszcza pokój\n");
static const char* CHAT_COMMAND_MSG   = "/msg";    // <nazwa_pokoju> <treść>- wysyła wiadomość\n");
static const char* CHAT_COMMAND_DM    = "/dm";     // <użytkownik> <treść>   - wiadomość prywatna\n");

// messages

static const char* CHAT_MESSAGE_KILL       = "KILL";
static const char* CHAT_MESSAGE_JOIN_ROOM  = "+room:%s";
static const char* CHAT_MESSAGE_LEAVE_ROOM = "-room:%s";

static const char* CHAT_ROOM_GENERAL = "general";

static const char* CHAT_TOPIC_GENERAL = "room:general";
static const char* CHAT_TOPIC_USER    = "user:%s";

#endif  // !ZMQ_CLIENT_COMMON_H
