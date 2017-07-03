#include "stream.h"
#include <stdlib.h>

struct _mdnsStreamBuf {
    struct pbuf *bufList;
    uint16_t currentPosition;
};

// create stream reader
mdnsStreamBuf *mdns_stream_new(struct pbuf *buffer) {
    mdnsStreamBuf *buf = malloc(sizeof(mdnsStreamBuf));
    pbuf_ref(buffer);

    buf->bufList = buffer;
    buf->currentPosition = 0;
}

bool mdns_advance_buffer(mdnsStreamBuf *buffer) {
    struct pbuf *next = buffer->bufList->next;
    if (next == NULL) {
        return false;
    }
    pbuf_ref(next);
    pbuf_free(buffer->bufList);
    buffer->bufList = next;
    buffer->currentPosition = 0;

    return true;
}

// read byte from stream
uint8_t mdns_stream_read8(mdnsStreamBuf *buffer) {
    if (buffer->currentPosition >= buffer->bufList->len - 1) {
        mdns_advance_buffer(buffer);
    }
    char *payload = buffer->bufList->payload;
    return payload[buffer->currentPosition++];
}

// read 16 bit int from stream
uint16_t mdns_stream_read16(mdnsStreamBuf *buffer) {
    return (mdns_stream_read8(buffer) << 8) \
            + mdns_stream_read8(buffer);
}

// read 32 bit int from stream
uint32_t mdns_stream_read32(mdnsStreamBuf *buffer) {
        return (mdns_stream_read8(buffer) << 24) \
            + (mdns_stream_read8(buffer) << 16) \
            + (mdns_stream_read8(buffer) << 8) \
            + mdns_stream_read8(buffer);
}

// caller has to free response
char *mdns_stream_read_string(mdnsStreamBuf *buffer, uint16_t len) {
    char *result = malloc(len + 1);
    for (uint8_t i = 0; i < len; i++) {
        result[i] = mdns_stream_read8(buffer);
    }
    result[len] = '\0';
}

// destroy stream reader
void mdns_stream_destroy(mdnsStreamBuf *buffer) {
    pbuf_free(buffer->bufList);
    free(buffer);
}
