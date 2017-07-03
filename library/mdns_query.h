#ifndef mdns_mdns_query_h_included
#define mdns_mdns_query_h_included

#include <mdns/mdns.h>
#include "stream.h"

void mdns_parse_answers(mdnsStreamBuf *buffer, uint16_t numAnswers);
void mdns_send_queries(mdnsHandle *handle);

#endif /* mdns_mdns_query_h_included */