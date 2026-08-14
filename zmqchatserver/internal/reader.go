// Package internal
package internal

import (
	"fmt"
	"log"
	"os"
	"zmqcharserver/api/chat/v1"

	// Ścieżka do wygenerowanego pakietu Protobuf

	"google.golang.org/protobuf/proto"
)

// Ścieżka do wygenerowanego pakietu Protobuf

func RunReader() {
	// 1. Odczytujemy surowe bajty z pliku
	data, err := os.ReadFile("../msg.bin")
	if err != nil {
		log.Fatalf("[Go] Błąd odczytu pliku: %v", err)
	}

	// 2. Tworzymy pustą strukturę koperty
	var envelope chat.MessageEnvelope

	// 3. Deserializacja bajtów
	if err := proto.Unmarshal(data, &envelope); err != nil {
		log.Fatalf("[Go] Błąd deserializacji: %v", err)
	}

	fmt.Printf("[Go] Odebrano MessageEnvelope! ID: %s\n", envelope.GetMessageId())

	// 4. Obsługa pola `oneof` za pomocą type-switch w Go
	switch payload := envelope.Payload.(type) {
	case *chat.MessageEnvelope_DirectMsg:
		dm := payload.DirectMsg
		fmt.Printf(" --> TYP: DirectMessage\n")
		fmt.Printf(" --> Od: %s\n", dm.GetSenderUsername())
		fmt.Printf(" --> Do: %s\n", dm.GetRecipientUsername())
		fmt.Printf(" --> Treść: %s\n", dm.GetContent())
		fmt.Printf(" --> Timestamp: %d\n", dm.GetTimestamp())

	case *chat.MessageEnvelope_RoomMsg:
		rm := payload.RoomMsg
		fmt.Printf(" --> TYP: RoomMessage [%s]\n", rm.GetRoomName())

	default:
		fmt.Println(" --> Nieznany typ wiadomości w kopercie")
	}
}
