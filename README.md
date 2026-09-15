# ZMQ CHAT

## Description

Simple Chat app that utilizes ZMQ wit C client and Go server.

## Client

Simple command line client in C.

### TODO

- [ ] separate input, output, logs in ui
  - [ ] add ncurses usage to ui
  - [ ] pass all input/output in ui to ncurses instead of stdin/stdout
  - [x] refactor logs - send all logs to ui via new zmq push pull socket and display
  - [ ] update Makefile - add ncurses lib to build

## Server

Simple server in Go.

### TODO

- [ ] ...
