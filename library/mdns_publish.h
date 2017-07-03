#ifndef mdns_mdns_publish_h_included
#define mdns_mdns_publish_h_included

#include <mdns/mdns.h>
#include "stream.h"

// this one is implemented in libplatform
uint16_t mdns_send_udp_packet(mdnsHandle *handle, char *data, uint16_t len);

// these are implemented here

// parse mdns query and react to it
void mdns_parse_query(mdnsStreamBuf *buffer, uint16_t numQueries);

// announce services
void mdns_announce(mdnsHandle *handle);

// send goodbye packet
void mdns_goodbye(mdnsHandle *handle);

#endif /* mdns_mdns_publish_h_included */