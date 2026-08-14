package server

import (
	"fmt"
	"sync"
	"time"
)

// ============================================================================
// 2. SESSION MANAGER (Zarządzanie sesjami użytkowników)
// ============================================================================

type SessionManager struct {
	mu    sync.RWMutex
	users map[string]string // sessionToken -> username
}

func NewSessionManager() *SessionManager {
	return &SessionManager{
		users: make(map[string]string),
	}
}

func (sm *SessionManager) CreateSession(username string) string {
	sm.mu.Lock()
	defer sm.mu.Unlock()

	token := fmt.Sprintf("token-%s-%d", username, time.Now().Unix())
	sm.users[token] = username
	return token
}
