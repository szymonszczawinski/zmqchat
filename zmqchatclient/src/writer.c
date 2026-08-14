#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "generated/chat.pb-c.h"  // Wygenerowany nagłówek dla C

int writer_run(void)
{
    // Create direct message
    Api__Chat__DirectMessage dm = API__CHAT__DIRECT_MESSAGE__INIT;
    dm.sender_username          = "szymon_s";
    dm.recipient_username       = "piotr_s";
    dm.content                  = "Hello! this is test message from C to Go";
    dm.timestamp                = (int64_t)time(NULL) * 1000;

    // Create Envelope
    Api__Chat__MessageEnvelope envelope = API__CHAT__MESSAGE_ENVELOPE__INIT;
    envelope.message_id                 = "msg-1001";

    // Set message type
    envelope.payload_case = API__CHAT__MESSAGE_ENVELOPE__PAYLOAD_DIRECT_MSG;
    envelope.direct_msg   = &dm;

    // Allocate memory
    size_t   packed_envelope_size = api__chat__message_envelope__get_packed_size(&envelope);
    uint8_t* envelope_buffer      = malloc(packed_envelope_size);
    if (!envelope_buffer)
    {
        perror("Memory allocation error");
        return 1;
    }
    // Serialize envelope to bytes
    api__chat__message_envelope__pack(&envelope, envelope_buffer);
    // 5. Zapis do pliku msg.bin
    FILE* fp = fopen("../msg.bin", "wb");
    if (!fp)
    {
        perror("Błąd otwarcia pliku");
        free(envelope_buffer);
        return 1;
    }
    fwrite(envelope_buffer, 1, packed_envelope_size, fp);
    fclose(fp);
    free(envelope_buffer);

    printf("[C] Wiadomosc zserializowana i zapisana do msg.bin (%zu bajtow)\n", packed_envelope_size);
    return EXIT_SUCCESS;
}
