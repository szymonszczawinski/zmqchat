// Package server
package server

import (
	"fmt"
	"log"
	"log/slog"
	"time"

	chat "zmqcharserver/api/chat/v1"

	zmq "github.com/pebbe/zmq4"
)

type ZmqServer struct {
	repSock  *ZmqSocket
	pubSock  *ZmqSocket
	sessions *SessionManager
	rooms    *RoomManager
}

func NewZmqServer(repAddr, pubAddr string) (*ZmqServer, error) {
	rep, err := NewZmqSocket(zmq.REP, repAddr)
	if err != nil {
		return nil, fmt.Errorf("REP socket init error: %w", err)
	}

	pub, err := NewZmqSocket(zmq.PUB, pubAddr)
	if err != nil {
		rep.Close()
		return nil, fmt.Errorf("PUB socket init error: %w", err)
	}

	return &ZmqServer{
		repSock:  rep,
		pubSock:  pub,
		sessions: NewSessionManager(),
		rooms:    NewRoomManager(),
	}, nil
}

func (srv *ZmqServer) Close() {
	srv.repSock.Close()
	srv.pubSock.Close()
}

// Start runs main server loop
func (srv *ZmqServer) Start() {
	slog.Info("[ZmqServer] server started:", "REP", srv.repSock.addr, "PUB", srv.pubSock.addr)

	for {
		reqEnv, err := srv.repSock.RecvEnvelope()
		if err != nil {
			slog.Error("[ZmqServer] receive envelope error", "err", err)
			continue
		}

		srv.handleEnvelope(reqEnv)
	}
}

// Router to handle received commands
func (srv *ZmqServer) handleEnvelope(env *chat.MessageEnvelope) {
	switch payload := env.Payload.(type) {

	case *chat.MessageEnvelope_LoginReq:
		srv.handleLogin(env.GetMessageId(), payload.LoginReq)

	case *chat.MessageEnvelope_JoinRoomReq:
		srv.handleJoinRoom(env.GetMessageId(), payload.JoinRoomReq)

	case *chat.MessageEnvelope_LeaveRoomReq:
		srv.handleLeaveRoom(env.GetMessageId(), payload.LeaveRoomReq)

	case *chat.MessageEnvelope_ListRoomsReq:
		srv.handleListRooms(env.GetMessageId())

	case *chat.MessageEnvelope_RoomMsg:
		srv.handleRoomMessage(env.GetMessageId(), payload.RoomMsg, env)

	case *chat.MessageEnvelope_DirectMsg:
		srv.handleDirectMessage(env.GetMessageId(), payload.DirectMsg, env)

	default:
		srv.sendAckResponse(env.GetMessageId(), chat.Status_STATUS_ERROR, "unknown request type")
	}
}

// --- METODY OBSŁUGI ZDARZEŃ BIZNESOWYCH ---

// --- HANDLERY POKOJOWE ---

func (srv *ZmqServer) handleLogin(msgID string, req *chat.LoginRequest) {
	token := srv.sessions.CreateSession(req.GetUsername())

	// Domyślnie dopisujemy usera do pokoju general
	srv.rooms.JoinRoom("general", req.GetUsername())

	srv.repSock.SendEnvelope(&chat.MessageEnvelope{
		MessageId: msgID,
		Payload: &chat.MessageEnvelope_LoginResp{
			LoginResp: &chat.LoginResponse{
				Status:       chat.Status_STATUS_OK,
				SessionToken: token,
			},
		},
	})
}

func (srv *ZmqServer) handleJoinRoom(msgID string, req *chat.JoinRoomRequest) {
	srv.rooms.JoinRoom(req.GetRoomName(), req.GetUsername())

	// Potwierdzenie dla dołączającego
	srv.sendAckResponse(msgID, chat.Status_STATUS_OK, fmt.Sprintf("Dołączono do pokoju #%s", req.GetRoomName()))

	// Powiadomienie pozostałych członków na szynie PUB
	notifEnv := &chat.MessageEnvelope{
		MessageId: fmt.Sprintf("notif-%d", time.Now().UnixNano()),
		Payload: &chat.MessageEnvelope_SystemNotif{
			SystemNotif: &chat.SystemNotification{
				Type:      chat.NotificationType_NOTIF_USER_JOINED,
				RoomName:  req.GetRoomName(),
				Message:   fmt.Sprintf("Użytkownik %s dołączył do pokoju!", req.GetUsername()),
				Timestamp: time.Now().UnixMilli(),
			},
		},
	}
	srv.pubSock.PublishTopicEnvelope(fmt.Sprintf("room:%s", req.GetRoomName()), notifEnv)
}

func (srv *ZmqServer) handleLeaveRoom(msgID string, req *chat.LeaveRoomRequest) {
	srv.rooms.LeaveRoom(req.GetRoomName(), req.GetUsername())

	srv.sendAckResponse(msgID, chat.Status_STATUS_OK, fmt.Sprintf("Opuszczono pokój #%s", req.GetRoomName()))

	notifEnv := &chat.MessageEnvelope{
		MessageId: fmt.Sprintf("notif-%d", time.Now().UnixNano()),
		Payload: &chat.MessageEnvelope_SystemNotif{
			SystemNotif: &chat.SystemNotification{
				Type:      chat.NotificationType_NOTIF_USER_LEFT,
				RoomName:  req.GetRoomName(),
				Message:   fmt.Sprintf("Użytkownik %s opuścił pokój.", req.GetUsername()),
				Timestamp: time.Now().UnixMilli(),
			},
		},
	}
	srv.pubSock.PublishTopicEnvelope(fmt.Sprintf("room:%s", req.GetRoomName()), notifEnv)
}

func (srv *ZmqServer) handleListRooms(msgID string) {
	roomsInfo := srv.rooms.GetRoomsInfo()

	srv.repSock.SendEnvelope(&chat.MessageEnvelope{
		MessageId: msgID,
		Payload: &chat.MessageEnvelope_ListRoomsResp{
			ListRoomsResp: &chat.ListRoomsResponse{
				Status: chat.Status_STATUS_OK,
				Rooms:  roomsInfo,
			},
		},
	})
}

func (srv *ZmqServer) handleRoomMessage(msgID string, req *chat.RoomMessage, rawEnv *chat.MessageEnvelope) {
	srv.sendAckResponse(msgID, chat.Status_STATUS_OK, "Wysłano")
	topic := fmt.Sprintf("room:%s", req.GetRoomName())
	srv.pubSock.PublishTopicEnvelope(topic, rawEnv)
}

func (srv *ZmqServer) handleDirectMessage(msgID string, req *chat.DirectMessage, rawEnv *chat.MessageEnvelope) {
	fmt.Printf("[ZmqServer] DM '%s' -> '%s': %s\n", req.GetSenderUsername(), req.GetRecipientUsername(), req.GetContent())

	srv.sendAckResponse(msgID, chat.Status_STATUS_OK, "Direct message delivered")

	topic := fmt.Sprintf("user:%s", req.GetRecipientUsername())
	srv.pubSock.PublishTopicEnvelope(topic, rawEnv)
}

// --- HELPERS ---
func (srv *ZmqServer) sendAckResponse(msgID string, status chat.Status, msg string) {
	srv.repSock.SendEnvelope(&chat.MessageEnvelope{
		MessageId: msgID,
		Payload: &chat.MessageEnvelope_Ack{
			Ack: &chat.GenericAck{
				Status:  status,
				Message: msg,
			},
		},
	})
}

func (srv *ZmqServer) sendOkResponse(msgID string) {
	srv.repSock.SendEnvelope(&chat.MessageEnvelope{
		MessageId: msgID,
		Payload: &chat.MessageEnvelope_LoginResp{
			LoginResp: &chat.LoginResponse{Status: chat.Status_STATUS_OK},
		},
	})
}

func (srv *ZmqServer) sendErrorResponse(msgID, errMsg string) {
	srv.repSock.SendEnvelope(&chat.MessageEnvelope{
		MessageId: msgID,
		Payload: &chat.MessageEnvelope_LoginResp{
			LoginResp: &chat.LoginResponse{
				Status:       chat.Status_STATUS_ERROR,
				ErrorMessage: errMsg,
			},
		},
	})
}

func RunServer() {
	server, err := NewZmqServer("tcp://*:5555", "tcp://*:5556")
	if err != nil {
		log.Fatalf("starting server error: %v", err)
	}
	defer server.Close()

	server.Start()
}
