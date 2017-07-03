#include <mdns/mdns.h>
#include <ctype.h>

#include "mdns_impl.h"
#include "server.h"

void mdns_server_task(void *userData) {
    mdnsHandle *handle = userData;
    mdnsTaskAction action = mdnsTaskActionNone;

    while (1) {
        if (xQueuePeek(handle->mdnsQueue, &action, 0) == pdPASS) {
            if (action == mdnsTaskActionDestroy) {
                continue; // destroy messages are for the caller, not for us
            }
            xQueueReceive(handle->mdnsQueue, &action, portMAX_DELAY);

            switch (action) {
                case mdnsTaskActionStart:
                    // start up service
                    mdns_join_multicast_group();
                    handle->pcb = mdns_listen(handle);
                    // and announce the services on the network
                    mdns_announce(handle);
                    break;

                case mdnsTaskActionStop:
                    // cleanly shut down, this means sending a goodbye message
                    mdns_goodbye(handle);
                    // shutdown socket
                    mdns_shutdown_socket(handle->pcb);
                    handle->pcb = NULL;
                    mdns_leave_multicast_group();
                    // notify parent and destroy this task
                    xQueueSendToBack(handle->mdnsQueue, (void *)mdnsTaskActionDestroy, portMAX_DELAY);
                    vTaskDelete(NULL);
                    break;

                case mdnsTaskActionRestart:
                    // just force an announcement
                    mdns_announce(handle);
                    break;
                
#if defined(MDNS_ENABLE_QUERY) && MDNS_ENABLE_QUERY
                case mdnsTaskActionQuery:
                    // send all registered queries
                    mdns_send_queries(handle);
                    break;
#endif /* MDNS_ENABLE_QUERY */

                default:
                    break;
            }
        }
    }
}

#if defined(MDNS_ENABLE_QUERY) && MDNS_ENABLE_QUERY
void mdns_add_query(mdnsHandle *handle, mdnsQueryHandle *query) {
    // TODO: mutex lock handle->queries
    handle->queries = realloc(handle->queries, sizeof(mdnsQueryHandle *) * (handle->numQueries + 1));
    handle->queries[handle->numQueries] = query;
    handle->numQueries++;

    xQueueSend(handle->mdnsQueue, (void *)mdnsTaskActionQuery, portMAX_DELAY);
}

void mdns_remove_query(mdnsHandle *handle, mdnsQueryHandle *query) {
    // TODO: mutex lock handle->queries
    for(uint8_t i = 0; i < handle->numQueries; i++) {
        if (handle->queries[i] == query) {
            for (uint8_t j = i + 1; j < handle->numQueries - 1; j++) {
                handle->queries[i] = handle->queries[j];
            }
        }
    }
    handle->queries = realloc(handle->queries, sizeof(mdnsQueryHandle *) * (handle->numQueries - 1));
    handle->numQueries--;
}
#endif /* MDNS_ENABLE_QUERY */

//
// API
//

mdnsHandle *mdns_create(char *hostname) {
    mdnsHandle *handle = malloc(sizeof(mdnsHandle));
    memset(handle, 0, sizeof(mdnsHandle));

    // duplicate hostname and convert to lowercase
    uint8_t hostnameLen = strlen(hostname);
    handle->hostname = malloc(hostnameLen + 1);
    for (uint8_t i = 0; i < hostnameLen; i++) {
        handle->hostname[i] = tolower(hostname[i]);
    }
    handle->hostname[hostnameLen] = '\0';

    handle->mdnsQueue = xQueueCreate(1, sizeof(mdnsTaskAction));
}

// Start broadcasting MDNS records
void mdns_start(mdnsHandle *handle) {
    xTaskCreate(mdns_server_task, "mdns", 100, handle, 3, &handle->mdnsTask);
    xQueueSendToBack(handle->mdnsQueue, (void *)mdnsTaskActionStart, portMAX_DELAY);
}

// Stop broadcasting MDNS records
void mdns_stop(mdnsHandle *handle) {
    xQueueSendToBack(handle->mdnsQueue, (void *)mdnsTaskActionStop, portMAX_DELAY);

    // wait for mdns service to stop
    mdnsTaskAction action = mdnsTaskActionNone;
    mdnsTaskAction *ptr = &action;
    while (action != mdnsTaskActionDestroy) {
        taskYIELD();
        xQueuePeek(handle->mdnsQueue, ptr, portMAX_DELAY);
    }
}

// Restart MDNS service (call on IP/Network change)
void mdns_restart(mdnsHandle *handle) {
    xQueueSendToBack(handle->mdnsQueue, (void *)mdnsTaskActionRestart, portMAX_DELAY);
}

// Destroy MDNS handle
void mdns_destroy(mdnsHandle *handle) {
    // shut down task
    if (handle->mdnsTask) {
        mdns_stop(handle);
    }

    // destroy all services
    uint8_t numServices = handle->numServices;
    handle->numServices = 0;
    for(uint8_t i = 0; i < numServices; i++) {
        mdns_service_destroy(handle->services[i]);
    }
    free(handle->services);

    // free hostname
    free(handle->hostname);

    // free complete handle
    free(handle);
}