#include <mdns/mdns.h>

#include "server.h"

#include <string.h>
#include <stdlib.h>

mdnsService *mdns_create_service(char *name, mdnsProtocol protocol, uint16_t port) {
    mdnsService *service = malloc(sizeof(mdnsService));
    memset(service, 0, sizeof(mdnsService));

    uint8_t nameLen = strlen(name);
    service->name = malloc(nameLen + 1);
    memcpy(service->name, name, nameLen + 1);

    service->protocol = protocol;
    service->port = port;
}

void mdns_service_add_txt(mdnsService *service, char *key, char *value) {
    service->txtRecords = realloc(service->txtRecords, sizeof(mdnsTxtRecord) * (service->numTxtRecords + 1));

    uint8_t len;
    
    len = strlen(key);
    service->txtRecords[service->numTxtRecords].name = malloc(len + 1);
    memcpy(service->txtRecords[service->numTxtRecords].name, key, len + 1);

    len = strlen(value);
    service->txtRecords[service->numTxtRecords].value = malloc(len + 1);
    memcpy(service->txtRecords[service->numTxtRecords].value, value, len + 1);

    service->numTxtRecords++;
}

void mdns_service_destroy(mdnsService *service) {
    for (uint8_t i = 0; i < service->numTxtRecords; i++) {
        free(service->txtRecords[i].name);
        free(service->txtRecords[i].value);
    }
    free(service->txtRecords);
    free(service->name);
    free(service);
}


#if defined(MDNS_ENABLE_PUBLISH) && MDNS_ENABLE_PUBLISH

void mdns_add_service(mdnsHandle *handle, mdnsService *service) {
    handle->services = realloc(handle->services, sizeof(mdnsService *) * (handle->numServices + 1));
    handle->services[handle->numServices] = service;
    handle->numServices++;

    xQueueSend(handle->mdnsQueue, (void *)mdnsTaskActionRestart, portMAX_DELAY);    
}

void mdns_remove_service(mdnsHandle *handle, mdnsService *service) {
    for(uint8_t i = 0; i < handle->numServices; i++) {
        if (handle->services[i] == service) {
            for (uint8_t j = i + 1; j < handle->numServices - 1; j++) {
                handle->services[i] = handle->services[j];
            }
        }
    }
    handle->services = realloc(handle->services, sizeof(mdnsService *) * (handle->numServices - 1));
    handle->numServices--;

    xQueueSend(handle->mdnsQueue, (void *)mdnsTaskActionRestart, portMAX_DELAY);    
}

#endif /* MDNS_ENABLE_PUBLISH */
