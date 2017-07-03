#ifndef mdns_mdns_publish_h_included
#define mdns_mdns_publish_h_included

#include <mdns/mdns.h>
#include "stream.h"

void mdns_parse_query(mdnsStreamBuf *buffer, uint16_t numQueries);
void mdns_announce(mdnsHandle *handle);
void mdns_goodbye(mdnsHandle *handle);

#endif /* mdns_mdns_publish_h_included */