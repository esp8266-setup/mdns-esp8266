#ifndef mdns_mdns_impl_h_included
#include <mdns/mdns.h>

#include "platform.h"
#include "stream.h"

#define MDNS_MULTICAST_ADDR 0xe00000fb
#define MDNS_MULTICAST_TTL 60 /* seconds */
#define MDNS_PORT 5353

typedef enum _mdnsResponseCode {
    responseCodeNoError = 0,
    responseCodeFormatError = 1,
    responseCodeServerFailure = 2,
    responseCodeNameError = 3,
    responseCodeNotImplemented = 4,
    responseCodeRefused = 5,
    responseCodeYXDomain = 6, // Domain name exists but it should not
    responseCodeZXRRSet = 7, // RR exists that should not
    responseCodeNXRRSet = 8, // RR should exist but does not
    responseCodeNotAuthoritative = 9,
    responseCodeNotInZone = 10
} mdnsResponseCode;

typedef enum _mdnsOpCode {
    opCodeQuery = 0,
    opCodeInverseQuery = 1,
    opCodeStatus = 2,
    opCodeReserved0 = 3,
    opCodeNotify = 4,
    opCodeUpdate = 5
} mdnsOpCode;

typedef struct __attribute__((packed)) _mdnsPacketFlags {
    bool isResponse:1;
    mdnsOpCode opCode:4;    // only opCodeQuery allowed here
    bool authoritative:1;   // responses are always authoritative
    bool isTruncated:1;     // responses are never truncated
    bool recursionWanted:1; // completely ignore this one for MDNS

    bool recursionAvailable:1; // completely ignore this one for MDNS
    bool authenticData:1;      // ignored
    bool checkingDisabled:1;   // ignored
    uint8_t padding:1;         // ignored
    mdnsResponseCode responseCode:4; // only responseCodeNoError allowed here
} mdnsPacketFlags;

typedef enum _mdnsRecordType {
    mdnsRecordTypeA = 0x01,
    mdnsRecordTypePTR = 0x0c,
    mdnsRecordTypeTXT = 0x10,
    mdnsRecordTypeSRV = 0x21,
    mdnsRecordTypeAAAA = 0x1c
} mdnsRecordType;

//
// Network related (this is implemented in libplatform)
//

// join multicast group
bool mdns_join_multicast_group(void);

// leave multicast group
bool mdns_leave_multicast_group(void);

// listen to multicast messages
mdnsUDPHandle *mdns_listen(mdnsHandle *handle);

// stop listening
void mdns_shutdown_socket(mdnsUDPHandle *pcb);

// parse and dispatch a packet (implemented here)
void mdns_parse_packet(mdnsHandle *handle, mdnsStreamBuf *buffer, ip_addr_t *ip, uint16_t port);

#endif /* mdns_mdns_impl_h_included */