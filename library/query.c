#include "query.h"

#include <mdns/mdns.h>
#include "server.h"



#if defined(MDNS_ENABLE_QUERY) && MDNS_ENABLE_QUERY

mdnsQueryHandle *mdns_query(mdnsHandle *handle, char *service, mdnsProtocol protocol, mdnsQueryCallback *callback) {
    mndsQueryHandle *qHandle = malloc(sizeof(mdnsQueryHandle));
    
    // copy over service name
    uint8_t serviceLen = strlen(service);
    qHandle->service = malloc(serviceLen + 1);
    memcpy(qHandle->service, service, serviceLen + 1);

    qHandle->protocol = protocol;
    qHandle->callback = callback;

    mdns_add_query(handle, qHandle);

    return qHandle;
}

void mdns_query_destroy(mdnsHandle *handle, mdnsQueryHandle *query) {
    mdns_remove_query(handle, query);

    free(query->service);
    free(query);
}

#endif /* MDNS_ENABLE_QUERY */
