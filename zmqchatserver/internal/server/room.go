package server

import (
	"sync"
	chat "zmqcharserver/api/chat/v1"
)

type Room struct {
	Name    string
	Members map[string]bool // zestaw (set) użytkowników w pokoju
}

type RoomManager struct {
	mu    sync.RWMutex
	rooms map[string]*Room
}

func NewRoomManager() *RoomManager {
	rm := &RoomManager{
		rooms: make(map[string]*Room),
	}
	// Domyślny pokój powitalny
	rm.rooms[RoomNameGeneral] = &Room{
		Name:    RoomNameGeneral,
		Members: make(map[string]bool),
	}
	return rm
}

func (rm *RoomManager) JoinRoom(roomName, username string) error {
	if username == "" {
		return ErrorIncorrectRoomName
	}
	if username == "" {
		return ErrorIncorrectUsername
	}
	rm.mu.Lock()
	defer rm.mu.Unlock()

	room, exists := rm.rooms[roomName]
	if !exists {
		room = &Room{
			Name:    roomName,
			Members: make(map[string]bool),
		}
		rm.rooms[roomName] = room
	}

	room.Members[username] = true
	return nil
}

func (rm *RoomManager) LeaveRoom(roomName, username string) error {
	if username == "" {
		return ErrorIncorrectRoomName
	}
	if username == "" {
		return ErrorIncorrectUsername
	}

	rm.mu.Lock()
	defer rm.mu.Unlock()

	if room, exists := rm.rooms[roomName]; exists {
		delete(room.Members, username)
		// Jeśli pokój robi się pusty i nie jest to "general", można go opcjonalnie usunąć:
		if len(room.Members) == 0 && roomName != RoomNameGeneral {
			delete(rm.rooms, roomName)
		}
	}
	return nil
}

func (rm *RoomManager) GetRoomsInfo() []*chat.RoomInfo {
	rm.mu.RLock()
	defer rm.mu.RUnlock()

	var result []*chat.RoomInfo
	for name, room := range rm.rooms {
		var members []string
		for user := range room.Members {
			members = append(members, user)
		}

		result = append(result, &chat.RoomInfo{
			Name:        name,
			MemberCount: int32(len(room.Members)),
			Members:     members,
		})
	}
	return result
}
