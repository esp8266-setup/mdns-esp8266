//
// PUBLISH
//

#if defined(MDNS_ENABLE_PUBLISH) && MDNS_ENABLE_PUBLISH

static uint16_t mdns_calculate_size(mdnsHandle *handle, mdnsRecordType query) {
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

static inline char *record_header(char *buffer, bool first, mdnsRecordType type, uint16_t ttl, uint16_t len) {
    if (!first) {
        // compressed fqdn pointer
        *buffer++ = 0xc0;
        *buffer++ = 0x0c;
    }
    // type
    *buffer++ = 0;
    *buffer++ = type;
    // class
    *buffer++ = 0x80; // cache buster flag
    *buffer++ = 0x01; // class: internet
    // ttl
    *buffer++ = ttl >> 8;
    *buffer++ = ttl & 0xff;
    // data length
    *buffer++ = len >> 8;
    *buffer++ = len & 0xff;

    return buffer;
}

static char *mdns_prepare_response(mdnsHandle *handle, mdnsRecordType query, uint16_t ttl, uint16_t transactionID, uint16_t *len) {
    *len = mdns_calculate_size(handle, query);
    char *buffer = malloc(*len);
    char *ptr = buffer;
    memset(buffer, 0, *len);

    // TODO: encode packet

    // transaction ID
    *ptr++ = transactionID >> 8;
    *ptr++ = transactionID & 0xff;

    // flags
    mdnsPacketFlags flags;
    memset(&flags, 0, 2);
    flags.isResponse = 1;
    flags.opCode = opCodeQuery;
    flags.authoritative = 1;
    flags.responseCode = responseCodeNoError;
    memcpy(ptr, &flags, 2);
    ptr += 2;

    // num questions (zero)
    *ptr++ = 0;  *ptr++ = 0;

    // num answers (one), we have at least the A record
    *ptr++ = 0;  *ptr++ = 1;

    // num authority RRs (zero)
    *ptr++ = 0;  *ptr++ = 0;

    // num Additional RRs
    uint8_t numRRs = 0;
    switch (query) {
        case mdnsRecordTypePTR:
            numRRs++;
            // fallthrough
        case mdnsRecordTypeSRV:
            numRRs += handle->numServices;
            // fallthrough
        case mdnsRecordTypeTXT:
            numRRs += handle->numServices; // one per service
            break;
    }
    *ptr++ = 0;
    *ptr++ = numRRs;

    // fqdn
    uint8_t hostnameLen = strlen(handle->hostname);
    *ptr++ = hostnameLen; // size prefix
    memcpy(ptr, handle->hostname, hostnameLen); // hostname
    ptr += hostnameLen;
    *ptr++ = 5; // strlen('local')
    memcpy(ptr, "local", 5); // local part
    ptr += 5;
    *ptr++ = 0; // terminator

    bool first = true;

    // records
    switch (query) {
        case mdnsRecordTypePTR:
            ptr = record_header(ptr, first, mdnsRecordTypePTR, ttl, hostnameLen);

            // packet data
            memcpy(ptr, handle->hostname, hostnameLen);
            ptr += hostnameLen;

            first = false;
            // fallthrough
        case mdnsRecordTypeSRV:
            for (uint8_t i = 0; i < handle->numServices; i++) {
                mdnsService *service = handle->services[i];
                uint8_t serviceNameLen = strlen(service->name);
                ptr = record_header(ptr, first, mdnsRecordTypeSRV, ttl, 6 + serviceNameLen + 1 /* . */ + hostnameLen);
                
                // prio
                *ptr++ = 0;
                *ptr++ = 0;

                // weight
                *ptr++ = 0;
                *ptr++ = 0;

                // port
                *ptr++ = service->port >> 8;
                *ptr++ = service->port & 0xff; 

                // service name
                memcpy(ptr, service->name, serviceNameLen);
                ptr += serviceNameLen;
                *ptr++ = '.';
                memcpy(ptr, handle->hostname, hostnameLen);
                ptr += hostnameLen;

                first = false;
            }
            // fallthrough
        case mdnsRecordTypeTXT:
            for (uint8_t i = 0; i < handle->numServices; i++) {
                mdnsService *service = handle->services[i];
                uint8_t serviceNameLen = strlen(service->name);

                uint8_t txtLen = 0;
                for(uint8_t j = 0; j < service->numTxtRecords; j++) {
                    txtLen += 1 /* len of txt record k/v pair */;
                    txtLen += strlen(service->txtRecords[j].name) + 1 /* = */;
                    txtLen += strlen(service->txtRecords[j].value);
                }

                ptr = record_header(
                    ptr, first, mdnsRecordTypeTXT, ttl,
                    serviceNameLen + 1 /* . */ + hostnameLen + txtLen
                );

                // service name
                memcpy(ptr, service->name, serviceNameLen);
                ptr += serviceNameLen;
                *ptr++ = '.';
                memcpy(ptr, handle->hostname, hostnameLen);
                ptr += hostnameLen;

                for(uint8_t j = 0; j < service->numTxtRecords; j++) {
                    uint8_t namLen = strlen(service->txtRecords[j].name);
                    uint8_t valLen = strlen(service->txtRecords[j].value);
                    *ptr++ = namLen + 1 + valLen;
                    memcpy(ptr, service->txtRecords[j].name, namLen);
                    ptr += namLen;
                    *ptr++ = '=';
                    memcpy(ptr, service->txtRecords[j].value, valLen);
                    ptr += valLen;                    
                }

                first = false;
            }
            break;
    }

    // always append an A record
    ptr = record_header(ptr, first, mdnsRecordTypeA, ttl, 4);
    memcpy(ptr, &handle->ip, 4);
    ptr += 4;

    LOG(TRACE, "Calculated len: %d, built len: %d", *len, ptr - buffer);

    return buffer;
}

static void send_mdns_response_packet(mdnsHandle *handle, uint16_t ttl, uint16_t transactionID) {
    uint16_t responseLen = 0;
    char *response = mdns_prepare_response(handle, mdnsRecordTypePTR, ttl, transactionID, &responseLen);
    struct pbuf * buf = pbuf_alloc(PBUF_IP, responseLen, PBUF_RAM);
    pbuf_take(buf, response, responseLen);
}

//
// API
//

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

void mdns_announce(mdnsHandle *handle) {
    // respond with our data, setting most significant bit in RRClass to update caches
    send_mdns_response_packet(handle, MDNS_MULTICAST_TTL, 0);
}

void mdns_goodbye(mdnsHandle *handle) {
    // send announce packet with TTL of zero
    send_mdns_response_packet(handle, 0, 0);
}

#endif /* MDNS_ENABLE_PUBLISH */