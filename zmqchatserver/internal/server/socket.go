package server

import (
	"fmt"
	chat "zmqcharserver/api/chat/v1"

	zmq "github.com/pebbe/zmq4"
	"google.golang.org/protobuf/proto"
)

type ZmqSocket struct {
	sock *zmq.Socket
	tp   zmq.Type
	addr string
}

func NewZmqSocket(tp zmq.Type, addr string) (*ZmqSocket, error) {
	sock, err := zmq.NewSocket(tp)
	if err != nil {
		return nil, fmt.Errorf("creating socket failed: %w", err)
	}

	if err := sock.Bind(addr); err != nil {
		sock.Close()
		return nil, fmt.Errorf("bind (%s) failed: %w", addr, err)
	}

	return &ZmqSocket{
		sock: sock,
		tp:   tp,
		addr: addr,
	}, nil
}

func (s *ZmqSocket) Close() {
	if s.sock != nil {
		s.sock.Close()
	}
}

// RecvEnvelope odbiera bajty i deserializuje je do MessageEnvelope
func (s *ZmqSocket) RecvEnvelope() (*chat.MessageEnvelope, error) {
	msgBytes, err := s.sock.RecvBytes(0)
	if err != nil {
		return nil, err
	}

	var env chat.MessageEnvelope
	if err := proto.Unmarshal(msgBytes, &env); err != nil {
		return nil, fmt.Errorf("unmarshal error: %w", err)
	}

	return &env, nil
}

// SendEnvelope serializuje i wysyła MessageEnvelope (np. dla REQ/REP)
func (s *ZmqSocket) SendEnvelope(env *chat.MessageEnvelope) error {
	outBytes, err := proto.Marshal(env)
	if err != nil {
		return fmt.Errorf("marshal error: %w", err)
	}

	_, err = s.sock.SendBytes(outBytes, 0)
	return err
}

// PublishTopicEnvelope wysyła wieloczęściową wiadomość PUB (Topic + Protobuf)
func (s *ZmqSocket) PublishTopicEnvelope(topic string, env *chat.MessageEnvelope) error {
	outBytes, err := proto.Marshal(env)
	if err != nil {
		return fmt.Errorf("marshal error: %w", err)
	}

	// Ramka 1: Temat (z flagą SNDMORE)
	if _, err := s.sock.Send(topic, zmq.SNDMORE); err != nil {
		return err
	}

	// Ramka 2: Payload Protobuf
	_, err = s.sock.SendBytes(outBytes, 0)
	return err
}
