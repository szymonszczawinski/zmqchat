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

// RemoveUser removes user from active sessions (both lastSeen and tokens)
func (sm *SessionManager) RemoveUser(username string) {
	if username == "" {
		return
	}
	sm.mu.Lock()
	defer sm.mu.Unlock()

	// 1. Usuwamy wpis z lastSeen
	delete(sm.lastSeen, username)

	// 2. Szukamy i usuwamy powiązany token z mapy users
	for token, u := range sm.users {
		if u == username {
			delete(sm.users, token)
			break
		}
	}
}

// GetDeadUsers returns a list of usernames that haven't sent a signal within the timeout duration
func (sm *SessionManager) GetDeadUsers(timeout time.Duration) []string {
	sm.mu.RLock()
	defer sm.mu.RUnlock()

	now := time.Now()
	var deadUsers []string

	for user, last := range sm.lastSeen {
		if now.Sub(last) > timeout {
			deadUsers = append(deadUsers, user)
		}
	}

	return deadUsers
}
