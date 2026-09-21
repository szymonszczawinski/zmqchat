// Package server
package server

import "fmt"

var (
	ErrorIncorrectUsername = fmt.Errorf("incorrect username")
	ErrorIncorrectRoomName = fmt.Errorf("incorrect room name")
)

const (
	MessagePong           string = "pong"
	RoomNameGeneral       string = "general"
	TopicNameRoomGeneral  string = "room:general"
	TopicNameRoomTemplate string = "room:%s"
)
