#ifndef mdns_server_h_included
#define mdns_server_h_included

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include <lwip/udp.h>

#include <mdns/mdns.h>

// MDNS Server handle
struct _mdnsHandle {
    // Hostname to broadcast
    char *hostname;

    // Services to broadcast
    mdnsService **services;
    uint8_t numServices;

    // freertos task and queue
    xTaskHandle mdnsTask;
    xQueueHandle mdnsQueue;

    // UDP port handle
    struct udp_pcb *pcb;

#if defined(MDNS_ENABLE_QUERY) && MDNS_ENABLE_QUERY
    mdnsQuery **queries;
    uint8_t numQueries;
#endif
};

typedef enum _mdnsTaskAction {
    mdnsTaskActionNone,
    mdnsTaskActionStart,
    mdnsTaskActionStop,
    mdnsTaskActionRestart,
#if defined(MDNS_ENABLE_QUERY) && MDNS_ENABLE_QUERY
    mdnsTaskActionQuery,
#endif
    mdnsTaskActionDestroy
} mdnsTaskAction;

#if defined(MDNS_ENABLE_QUERY) && MDNS_ENABLE_QUERY
void mdns_add_query(mdnsHandle *handle, mdnsQueryHandle *query);
void mdns_remove_query(mdnsHandle *handle, mdnsQueryHandle *query);
#endif /* MDNS_ENABLE_QUERY */

#endif /* mdns_server_h_included */