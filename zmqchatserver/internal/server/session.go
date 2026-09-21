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

// CreateSession creates new session for given username
func (sm *SessionManager) CreateSession(username string) (string, error) {
	if username == "" {
		return "", ErrorIncorrectUsername
	}
	sm.mu.Lock()
	defer sm.mu.Unlock()

	token := fmt.Sprintf("token-%s-%d", username, time.Now().Unix())
	sm.users[token] = username
	return token, nil
}

// TouchUser update last user activity
func (sm *SessionManager) TouchUser(username string) error {
	if username == "" {
		return ErrorIncorrectUsername
	}
	sm.mu.Lock()
	defer sm.mu.Unlock()
	sm.lastSeen[username] = time.Now()
	return nil
}

// RemoveUser removes user from active sessions (both lastSeen and tokens)
func (sm *SessionManager) RemoveUser(username string) error {
	if username == "" {
		return ErrorIncorrectUsername
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
	return nil
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
