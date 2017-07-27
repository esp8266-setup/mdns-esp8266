#include "dns.h"
#include "tools.h"
#include "server.h"
#include "mdns_network.h"

#include "debug.h"

static inline uint16_t sizeof_record_header(char *fqdn) {
    uint16_t size = 0;

    uint8_t fqdnLen = strlen(fqdn);

    size += fqdnLen + 1;
    size++; // terminator
    size += 2; // type
    size += 2; // class
    size += 4; // ttl
    size += 2; // data length

    return size;
}

uint16_t mdns_sizeof_PTR(char *hostname, mdnsService *service) {
    LOG(TRACE, "mdns: sizing PTR record");

    uint16_t size = 0;
    char *fqdn = mdns_make_service_name(service); // _type._protocol.local
    size += sizeof_record_header(fqdn);
    free(fqdn);

    // packet data
    size += strlen(hostname);

    LOG(TRACE, "mdns: %d", size);
    return size;
}

uint16_t mdns_sizeof_SRV(char *hostname, mdnsService **services, uint8_t numServices, mdnsService *serviceOrNull) {
    LOG(TRACE, "mdns: sizing SRV record");
    uint16_t size = 0;

    for (uint8_t i = 0; i < numServices; i++) {
        mdnsService *service = services[i];
        if (serviceOrNull) {
            service = serviceOrNull; // service override
        }
        uint8_t serviceNameLen = strlen(service->name);

        char *fqdn = mdns_make_fqdn(hostname, service); // Hostname._service._protocol.local
        size += sizeof_record_header(fqdn);
        free(fqdn);
        
        size += 2; // prio
        size += 2; // weight
        size += 2; // port

        // target
        fqdn = mdns_make_local(hostname);
        size += strlen(fqdn);
        free(fqdn);

        if (serviceOrNull) {
            break; // short circuit if we only should send one service
        }
    }

    LOG(TRACE, "mdns: %d", size);
    return size;
}

uint16_t mdns_sizeof_TXT(char *hostname, mdnsService **services, uint8_t numServices, mdnsService *serviceOrNull) {
    LOG(TRACE, "mdns: sizing TXT record");
    uint16_t size = 0;

    // Hostname._servicetype._protocol.local
    for (uint8_t i = 0; i < numServices; i++) {
        mdnsService *service = services[i];

        if (serviceOrNull) {
            service = serviceOrNull; // service override
        }

        uint8_t txtLen = 0;
        for(uint8_t j = 0; j < service->numTxtRecords; j++) {
            txtLen += 1 /* len of txt record k/v pair */;
            txtLen += strlen(service->txtRecords[j].name) + 1 /* = */;
            txtLen += strlen(service->txtRecords[j].value);
        }
        if (txtLen == 0) {
            txtLen = 1; // NULL byte sentinel at least
        }

        char *fqdn = mdns_make_fqdn(hostname, service); // Servicename._type._protocol.local
        size += sizeof_record_header(fqdn);
        free(fqdn);

        for(uint8_t j = 0; j < service->numTxtRecords; j++) {
            uint8_t namLen = strlen(service->txtRecords[j].name);
            uint8_t valLen = strlen(service->txtRecords[j].value);
            size++; // size prefix
            size += namLen;
            size++; // =
            size += valLen;
        }

        if (txtLen == 0) {
            size++; // empty txt record
        }

        if (serviceOrNull) {
            break; // short circuit if we only should send one service
        }
    }

    LOG(TRACE, "mdns: %d", size);
    return size;
}

uint16_t mdns_sizeof_A(char *hostname) {
    LOG(TRACE, "mdns: sizing A record");
    uint16_t size = 0;

    // fqdn
    char *fqdn = mdns_make_local(hostname);
    size += sizeof_record_header(fqdn);
    free(fqdn);

    // ip address
    size += 4;

    LOG(TRACE, "mdns: %d", size);
    return size;
}

uint16_t mdns_sizeof_AAAA() {

}

static inline char *record_header(char *buffer, char *fqdn, mdnsRecordType type, uint16_t ttl, uint16_t len) {
    uint8_t fqdnLen = strlen(fqdn);
    
    // calculate lengts
    uint8_t parts[5];
    uint8_t partIndex = 0;
    for (uint8_t i = 0; i < fqdnLen; i++) {
        if (fqdn[i] == '.') {
            if (partIndex > 0) {
                parts[partIndex] = i - parts[partIndex - 1];
            } else {
                parts[partIndex] = i;
            }
            partIndex++;
        }
    }
    parts[partIndex] = fqdnLen - parts[partIndex - 1] - 1;

    // write part entries
    partIndex = 0;
    for (uint8_t i = 0; i < fqdnLen; i++) {
        if ((i == 0) || (fqdn[i] == '.')) {
            LOG(TRACE, "%d part: %d bytes", partIndex, parts[partIndex]);
            *buffer++ = parts[partIndex++];
        }
        if (fqdn[i] != '.') {
            *buffer++ = fqdn[i];
        }
    }
    *buffer++ = 0; // terminator

    // type
    *buffer++ = 0;
    *buffer++ = type;
    // class
    *buffer++ = 0x80; // cache buster flag
    *buffer++ = 0x01; // class: internet
    // ttl
    *buffer++ = ttl >> 24;
    *buffer++ = ttl >> 16;
    *buffer++ = ttl >> 8;
    *buffer++ = ttl & 0xff;
    // data length
    *buffer++ = len >> 8;
    *buffer++ = len & 0xff;

    return buffer;
}

char *mdns_make_PTR(char *buffer, uint16_t ttl, char *hostname, mdnsService *service) {
    LOG(TRACE, "mdns: Building PTR record");
    char *ptr = buffer;

    char *fqdn = mdns_make_service_name(service); // _type._protocol.local
    ptr = record_header(ptr, fqdn, mdnsRecordTypePTR, ttl, strlen(hostname));
    free(fqdn);

    // packet data
    memcpy(buffer, hostname, strlen(hostname));
    ptr += strlen(hostname);
    
    return ptr;
}

char *mdns_make_SRV(char *buffer, uint16_t ttl, char *hostname, mdnsService **services, uint8_t numServices, mdnsService *serviceOrNull) {
    LOG(TRACE, "mdns: Building %d SRV records", numServices);
    char *ptr = buffer;

    for (uint8_t i = 0; i < numServices; i++) {
        mdnsService *service = services[i];
        if (serviceOrNull) {
            service = serviceOrNull; // service override
        }
        uint8_t serviceNameLen = strlen(service->name);

        char *fqdn = mdns_make_fqdn(hostname, service); // Hostname._service._protocol.local
        ptr = record_header(ptr, fqdn, mdnsRecordTypeSRV, ttl, strlen(hostname) + 6 /* .local */);
        free(fqdn);
        
        // prio
        *ptr++ = 0;
        *ptr++ = 0;

        // weight
        *ptr++ = 0;
        *ptr++ = 0;

        // port
        *ptr++ = service->port >> 8;
        *ptr++ = service->port & 0xff; 

        // target
        fqdn = mdns_make_local(hostname);
        memcpy(ptr, fqdn, strlen(fqdn));
        free(fqdn);

        if (serviceOrNull) {
            break; // short circuit if we only should send one service
        }
    }

    return ptr;
}

char *mdns_make_TXT(char *buffer, uint16_t ttl, char *hostname, mdnsService **services, uint8_t numServices, mdnsService *serviceOrNull) {
    LOG(TRACE, "mdns: Building TXT records for %d services", numServices);
    char *ptr = buffer;

    // Hostname._servicetype._protocol.local
    for (uint8_t i = 0; i < numServices; i++) {
        mdnsService *service = services[i];

        if (serviceOrNull) {
            service = serviceOrNull; // service override
        }

        uint8_t txtLen = 0;
        for(uint8_t j = 0; j < service->numTxtRecords; j++) {
            txtLen += 1 /* len of txt record k/v pair */;
            txtLen += strlen(service->txtRecords[j].name) + 1 /* = */;
            txtLen += strlen(service->txtRecords[j].value);
        }
        if (txtLen == 0) {
            txtLen = 1; // NULL byte sentinel at least
        }

        char *fqdn = mdns_make_fqdn(hostname, service); // Servicename._type._protocol.local
        ptr = record_header(ptr, fqdn, mdnsRecordTypeTXT, ttl, txtLen);
        free(fqdn);

        LOG(TRACE, "mdns: Building %d TXT records", service->numTxtRecords);
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

        if (txtLen == 0) {
            *ptr++ = 0; // empty txt record
        }

        if (serviceOrNull) {
            break; // short circuit if we only should send one service
        }
    }

    return ptr;
}

char *mdns_make_A(char *buffer, uint16_t ttl, char *hostname, struct ip_addr ip) {
    LOG(TRACE, "mdns: Building A record");
    char *ptr = buffer;

    // fqdn
    char *fqdn = mdns_make_local(hostname);
    ptr = record_header(ptr, fqdn, mdnsRecordTypeA, ttl, 4);
    free(fqdn);

    // ip address
    memcpy(ptr, &ip, 4);
    ptr += 4;

    return ptr;
}

char *mdns_make_AAAA() {

}