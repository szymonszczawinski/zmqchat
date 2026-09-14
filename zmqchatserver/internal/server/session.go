package server

import (
	"fmt"
	"sync"
	"time"
)

// ============================================================================
// 2. SESSION MANAGER (user sessions management)
// ============================================================================

type SessionManager struct {
	mu       sync.RWMutex
	users    map[string]string    // sessionToken -> username
	lastSeen map[string]time.Time // username -> timestamp
}

func NewSessionManager() *SessionManager {
	return &SessionManager{
		users:    make(map[string]string),
		lastSeen: make(map[string]time.Time),
	}
}

func (sm *SessionManager) CreateSession(username string) string {
	sm.mu.Lock()
	defer sm.mu.Unlock()

	token := fmt.Sprintf("token-%s-%d", username, time.Now().Unix())
	sm.users[token] = username
	return token
}

// TouchUser update last user activity
func (sm *SessionManager) TouchUser(username string) {
	if username == "" {
		return
	}
	sm.mu.Lock()
	defer sm.mu.Unlock()
	sm.lastSeen[username] = time.Now()
}
