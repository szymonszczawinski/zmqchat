# Faza 1: Baza danych i Kontrakt (Protobuf)

Cel: Stworzenie wspólnego języka, którym będą się porozumiewać Go i C.

## Krok 1.1: Konfiguracja środowiska i narzędzi

Zainstalowanie protoc (kompilatora Protobuf).

Instalacja wtyczki dla Go (protoc-gen-go) oraz dla C (protobuf-c).

Instalacja bibliotek ZeroMQ (libzmq dla C oraz pebbe/zmq4 dla Go).

## Krok 1.2: Projekt pliku chat.proto

Zdefiniowanie uniwersalnej „koperty” (ChatMessage Envelope), która pozwoli rozróżnić typ przesłanej wiadomości.

Stworzenie struktur dla podstawowych akcji: LoginRequest/LoginResponse, RoomMessage oraz SystemNotification.

## Krok 1.3: Generowanie kodu

Kompilacja pliku .proto do plików źródłowych C (.pb-c.h, .pb-c.c) oraz Go (.pb.go).

Napisanie krótkiego testu jednostkowego w obu językach (serializacja prostej struktury w C -> zapis do pliku -> odczyt i deserializacja w Go).

# Faza 2: Pierwszy strzał (Synchroniczne REQ / REP)

Cel: Nawiązanie podstawowej komunikacji klient (C) -> serwer (Go) i obsługa logowania.

## Krok 2.1: Prosty serwer REQ/REP w Go

Utworzenie gniazda ZMQ_REP na wybranym porcie (np. tcp://*:5555).

Pętla odbierająca bajty, deserializująca je przez Protobuf, wypisująca tekst w konsoli i odsyłająca odpowiedź statusową.

## Krok 2.2: Prosty klient REQ/REP w C

Utworzenie gniazda ZMQ_REQ połączonego z serwerem.

Zbudowanie struktury LoginRequest w C, zserializowanie jej do bufora, wysłanie przez ZMQ i odebranie odpowiedzi.

## Krok 2.3: Obsługa logiki sesji po stronie Go

Dodanie do serwera prostej pamięci w podręcznej (mapa map[string]User) do przechowywania zalogowanych użytkowników i ich statusu.

# Faza 3: Kanał wiadomości na żywo (Asynchroniczne PUB / SUB)

Cel: Budowa mechanizmu pokojów i rozgłaszania wiadomości w czasie rzeczywistym.

## Krok 3.1: Dodanie szyny PUB po stronie serwera Go

Otwarcie drugiego gniazda ZMQ – ZMQ_PUB (np. na porcie tcp://*:5556).

Utworzenie wewnętrznego kanału (go channel) do przekazywania wiadomości z gniazda odbierającego (REQ/DEALER) do gniazda rozgłaszającego (PUB).

## Krok 3.2: Odbiornik SUB po stronie klienta C

+-----------------------------------------------------------------+
|                         KLIENT W C                              |
|                                                                 |
|  +------------------------+        +--------------------------+ |
|  |     Główny Wątek       |        |   Wątek Tła (pthread)    | |
|  |   (Pętla CLI / REQ)    |        |        (ZMQ_SUB)         | |
|  +------------------------+        +--------------------------+ |
|              |                                   |              |
+--------------|-----------------------------------|--------------+
               | (tcp://localhost:5555)            | (tcp://localhost:5556)
               v                                   v
    [Gniazdo REP w Go]                  [Gniazdo PUB w Go]

Dodanie drugiego wątku lub użycie zmq_poll w C, aby klient mógł równolegle nasłuchiwać na gnieździe ZMQ_SUB.

Przetestowanie filtrowania tematów (np. subskrypcja prefiksu "room:general").

## Krok 3.3: Przesyłanie i rozgłaszanie wiadomości z pokoju

Klient C wysyła wiadomość do pokoju.

Serwer Go odbiera ją, dokleja nagłówek tematu (np. room:general [dane_protobuf]) i publikuje przez ZMQ_PUB.

Klient C odbiera ramkę, odcina prefiks tematu, deserializuje Protobuf i wyświetla na ekranie.

# Faza 4: Doprofilowanie i Architektura Wyrównana (ZMQ Router/Dealer)

Cel: Zamiana sztywnej architektury REQ/REP na skalowalny model asynchroniczny i ulepszenie interfejsu.

## Krok 4.1: Przejście z REQ/REP na ROUTER/DEALER

Zastąpienie ZMQ_REP na serwerze przez ZMQ_ROUTER (aby serwer mógł obsługiwać wielu klientów bez blokowania się).

Zastąpienie ZMQ_REQ na kliencie przez ZMQ_DEALER (asynchroniczne wysyłanie bez konieczności natychmiastowego czekania na odpowiedź).

## Krok 4.2: Pokoje dynamiczne i listy użytkowników

Obsługa komend klienta dołączania/opuszczania pokojów (ZMQ_SUBSCRIBE / ZMQ_UNSUBSCRIBE).

Rozgłaszanie powiadomień systemowych (np. „Użytkownik X dołączył do pokoju”).

## Krok 4.3: Interfejs CLI w C i porządki

Stworzenie pętli wprowadzania komend w C (np. /join #room, /msg treść, /quit).

Ładne parsowanie i czyszczenie pamięci (poprawne zwalnianie struktur protobuf-c oraz zamykanie gniazd zmq_close).
