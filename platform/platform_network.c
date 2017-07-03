#include "platform.h"

#include "mdns_network.h"
#include "stream.h"
#include "debug.h"

//
// private
//

void mdns_recv_callback(void *arg, struct udp_pcb *pcb, struct pbuf *buf, ip_addr_t *ip, uint16_t port) {
    mdnsHandle *handle = (mdnsHandle *)arg;

    // make a stream buffer
    mdnsStreamBuf *buffer = mdns_stream_new(buf);

    // call parser
    mdns_parse_packet(handle, buffer, ip, port);

    // clean up our stream reader
    mdns_stream_destroy(buffer);
}

//
// API
//

bool mdns_join_multicast_group(void) {
    ip_addr_t multicast_addr;
    multicast_addr.addr = (uint32_t) MDNS_MULTICAST_ADDR;

    if (igmp_joingroup(IP_ADDR_ANY, &multicast_addr)!= ERR_OK) {
        return false;
    }

    return true;
}

bool mdns_leave_multicast_group(void) {
    ip_addr_t multicast_addr;
    multicast_addr.addr = (uint32_t) MDNS_MULTICAST_ADDR;

    if (igmp_leavegroup(IP_ADDR_ANY, &multicast_addr)!= ERR_OK) {
        return false;
    }

    return true;    
}

mdnsUDPHandle *mdns_listen(mdnsHandle *handle) {
    ip_addr_t multicast_addr;
    multicast_addr.addr = (uint32_t) MDNS_PORT;

    struct udp_pcb *pcb = udp_new();
    pcb->ttl = MDNS_MULTICAST_TTL;

    err_t err = udp_bind(pcb, &multicast_addr, MDNS_PORT);
    if (err != ERR_OK) {
        LOG(ERROR, "Could not listen to UDP port");
        return NULL;
    }

    udp_recv(pcb, mdns_recv_callback, (void *)handle);

    udp_connect(pcb, &multicast_addr, MDNS_PORT);
    return pcb;
}

void mdns_shutdown_socket(mdnsUDPHandle *pcb) {
    udp_disconnect(pcb);
    udp_remove(pcb);
}
