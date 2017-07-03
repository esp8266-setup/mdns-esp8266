#include "debug.h"
#include "mdns_impl.h"
#include "stream.h"
#include "server.h"

void mdns_recv_callback(void *arg, struct udp_pcb *pcb, struct pbuf *buf, ip_addr_t *ip, uint16_t port);

//
// Network stuff
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

struct udp_pcb *mdns_listen(mdnsHandle *handle) {
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

void mdns_shutdown_socket(struct udp_pcb *pcb) {
    udp_disconnect(pcb);
    udp_remove(pcb);
}

//
// MDNS parser
//

void mdns_parse_answers(mdnsStreamBuf *buffer, uint16_t numAnswers) {
    char *serviceName[2];

    // TODO: Parse Additional RRs

    // Assume that the PTR answer always comes first and
    // that it is always accompanied by a TXT, SRV,
    // AAAA (optional) and A answer in the same packet.
    if (numAnswers < 4) {
        return;
    }

    while (numAnswers--) {
        // Read FQDN
        uint8_t stringsRead = 0;
        do {
            uint8_t len = mdns_stream_read8(buffer);
            if (len & 0xC0) { // Compressed pointer (not supported)
                (void)mdns_stream_read8(buffer);
                break;
            }
            if (len == 0x00) { // End of name
                break;
            }
            if (stringsRead > 3) {
                return;
            }
            serviceName[stringsRead] = mdns_stream_read_string(buffer, len);
            stringsRead++;
        } while (true);


        mdnsRecordType answerType = mdns_stream_read16(buffer);
        uint16_t answerClass = mdns_stream_read16(buffer) & 0x7f; // mask out top bit: cache buster flag
        uint32_t answerTtl = mdns_stream_read32(buffer);
        uint16_t dataLength = mdns_stream_read16(buffer);

        switch(answerType) {
            case mdnsRecordTypeA: { // IPv4 Address
                uint32_t answerIP = mdns_stream_read32(buffer);
                // TODO: do something with IP
            }
            case mdnsRecordTypePTR: { // Reverse lookup aka. Hostname
                char *hostname = mdns_stream_read_string(buffer, dataLength);
                if (hostname[0] == 0xc0) {
                    free(hostname); // Compressed pointer (not supported)
                }
                // TODO: do something with hostname
                free(hostname);
            }
            case mdnsRecordTypeTXT: { // Text records
                char **txt = NULL;
                uint8_t numRecords = 0;

                for (uint16_t i; i < dataLength; i++) {
                    uint8_t len = mdns_stream_read8(buffer);
                    if (len == 0) {
                        break; // no record
                    }
                    char *record = mdns_stream_read_string(buffer, len);
                    if (record[0] == '=') { // oer RFC 6763 Section 6.4
                        free(record);
                        continue;
                    }
                    i += len;

                    txt = realloc(txt, sizeof(char *) * (numRecords + 1));
                    txt[numRecords++] = record;
                }

                // TODO: do something with txt records

                if (numRecords > 0) {
                    for (uint8_t i; i < numRecords; i++) {
                        free(txt[i]);
                    }
                    free(txt);
                }
            }
            case mdnsRecordTypeSRV: { // Service records
                uint16_t prio = mdns_stream_read16(buffer);
                uint16_t weight = mdns_stream_read16(buffer);
                uint16_t port = mdns_stream_read16(buffer);

                uint16_t hostnameLen = mdns_stream_read16(buffer);
                char *hostname = mdns_stream_read_string(buffer, hostnameLen);
                // TODO: do something with service data
                free(hostname);               
            }
            case mdnsRecordTypeAAAA:
            default:
                // Ignore these, just skip over the buffer
                for (uint16_t i = 0; i < dataLength; i++) {
                    (void)mdns_stream_read8(buffer);
                }
                break;
        }
    }
    return;    
}

void mdns_parse_query(mdnsStreamBuf *buffer, uint16_t numQueries) {
    // we have to react to:
    // - domain name queries
    // - service discovery queries to one of our registered service
    // - browsing queries: _services._dns-sd._udp

    // uint16_t currentType;
    // uint16_t currentClass;

    // int numQuestions = packetHeader[2];
    // if(numQuestions > 4) numQuestions = 4;
    // uint16_t questions[4];
    // int question = 0;

    // while(numQuestions--){
    //     currentType = _conn_read16();
    //     if(currentType & MDNS_NAME_REF){ //new header handle it better!
    //         currentType = _conn_read16();
    //     }
    //     currentClass = _conn_read16();
    //     if(currentClass & MDNS_CLASS_IN)
    //         questions[question++] = currentType;

    //     if(numQuestions > 0){
    //         if(_conn_read16() != 0xC00C){//new question but for another host/service
    //             _conn->flush();
    //             numQuestions = 0;
    //         }
    //     }
    // }

    // uint8_t questionMask = 0;
    // uint8_t responseMask = 0;
    // for(i=0;i<question;i++){
    //     if(questions[i] == MDNS_TYPE_A) {
    //         questionMask |= 0x1;
    //         responseMask |= 0x1;
    //     } else if(questions[i] == MDNS_TYPE_SRV) {
    //         questionMask |= 0x2;
    //         responseMask |= 0x3;
    //     } else if(questions[i] == MDNS_TYPE_TXT) {
    //         questionMask |= 0x4;
    //         responseMask |= 0x4;
    //     } else if(questions[i] == MDNS_TYPE_PTR) {
    //         questionMask |= 0x8;
    //         responseMask |= 0xF;
    //     }
    // }

    // IPAddress interface = _getRequestMulticastInterface();
    // return _replyToInstanceRequest(questionMask, responseMask, serviceName, protoName, servicePort, interface);

}

uint16_t mdns_calculate_size(mdnsHandle *handle, mdnsRecordType query) {
    uint8_t hostnameLen = strlen(handle->hostname);
    uint16_t size = 12; // header

    // FQDN
    uint8_t fqdnLen = hostnameLen + 1 /* size prefix */ + 7 /* size prefix + '.local' + 0x00 */;

    switch (query) {
        case mdnsRecordTypePTR:
            size += fqdnLen;
            size += 2 /* type code */ + 2 /* class code */ + 2 /* ttl */ + 2 /* data len */;
            size += hostnameLen;

            fqdnLen = 2;
            // fallthrough
        case mdnsRecordTypeSRV:
            for (uint8_t i = 0; i < handle->numServices; i++) {
                uint8_t serviceNameLen = strlen(handle->services[i]->name) + 1 /* . */ + hostnameLen;
                size += fqdnLen;
                size += 2 /* type code */ + 2 /* class code */ + 2 /* ttl */ + 2 /* data len */;
                size += 2 /* priority */ + 2 /* weight */ + 2 /* port */ + serviceNameLen;

                fqdnLen = 2;
            }
            // fallthrough
        case mdnsRecordTypeTXT:
            for (uint8_t i = 0; i < handle->numServices; i++) {
                mdnsService *service = handle->services[i];

                uint8_t serviceNameLen = strlen(service->name) + 1 /* . */ + hostnameLen;
                size += serviceNameLen;
                size += 2 /* type code */ + 2 /* class code */ + 2 /* ttl */ + 2 /* data len */;

                for(uint8_t j = 0; j < service->numTxtRecords; j++) {
                    size += 1 /* len of txt record k/v pair */;
                    size += strlen(service->txtRecords[j].name) + 1 /* = */;
                    size += strlen(service->txtRecords[j].value);
                }


                fqdnLen = 2;
            }
            break;
    }

    // always append an A record
    size += fqdnLen;
    size += 2 /* type code */ + 2 /* class code */ + 2 /* ttl */ + 2 /* data len */;
    size += 4 /* ip address */;
}

char *mdns_prepare_response(mdnsHandle *handle, mdnsRecordType query, uint16_t ttl, uint16_t transactionID, uint16_t *len) {
    *len = mdns_calculate_size(handle, query);
    char *buffer = malloc(*len);
    memset(buffer, 0, *len);

    // TODO: encode packet
}

void mdns_query(void) {
    // send query, setting most significant bit in QClass to zero to get multicast responses

    // A DNS-SD query is just a PTR query to _<service>._<protocol>.local
    // -> Usually the server will send additional RRs that contain SRV, TXT and at least one A or AAAA
    // -> If that does not happen we have to send another query for those with the data from the PTR
    // A MDNS query is just a A query to <hostname>.local
}

//
// Callback
//

void mdns_recv_callback(void *arg, struct udp_pcb *pcb, struct pbuf *buf, ip_addr_t *ip, uint16_t port) {
    mdnsHandle *handle = (mdnsHandle *)arg;
    
    mdnsStreamBuf *buffer = mdns_stream_new(buf);
    uint16_t transactionID = mdns_stream_read16(buffer);
    uint16_t flagsTmp = mdns_stream_read16(buffer);
    mdnsPacketFlags flags;
    memcpy(&flags, &flagsTmp, 2);

    // MDNS only supports opCode 0 -> query, and non-error response codes
    if ((flags.opCode != opCodeQuery) || (flags.responseCode != responseCodeNoError)) {
        mdns_stream_destroy(buffer);
        return;
    }

    uint16_t numQuestions = mdns_stream_read16(buffer);
    uint16_t numAnswers = mdns_stream_read16(buffer);
    uint16_t numAuthorityRR = mdns_stream_read16(buffer);
    uint16_t numAdditionalRR = mdns_stream_read16(buffer);

    // MDNS Answer flag set -> read answers
    if (flags.isResponse) {
        // Read answers, additional sideloaded records will be appended after the answer
        // so we can parse them with one parser
        mdns_parse_answers(buffer, numAnswers + numAdditionalRR);
    } else {
        mdns_parse_query(buffer, numQuestions);
    }

    // clean up our stream reader
    mdns_stream_destroy(buffer);
}

//
// API
//

void send_mdns_response_packet(mdnsHandle *handle, uint16_t ttl, uint16_t transactionID) {
    uint16_t responseLen = 0;
    char *response = mdns_prepare_response(handle, mdnsRecordTypePTR, ttl, transactionID, &responseLen);
    struct pbuf * buf = pbuf_alloc(PBUF_IP, responseLen, PBUF_RAM);
    pbuf_take(buf, response, responseLen);
}

void mdns_announce(mdnsHandle *handle) {
    // respond with our data, setting most significant bit in RRClass to update caches
    send_mdns_response_packet(handle, MDNS_MULTICAST_TTL, 0);
}

void mdns_goodbye(mdnsHandle *handle) {
    // send announce packet with TTL of zero
    send_mdns_response_packet(handle, 0, 0);
}
