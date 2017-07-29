#include <esp_common.h>

#include <strings.h>
#include <stdlib.h>
#include <unistd.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <mdns/mdns.h>

mdnsHandle *mdns;

void startup(void *userData);

/******************************************************************************
 * FunctionName : user_rf_cal_sector_set
 * Description  : SDK just reversed 4 sectors, used for rf init data and paramters.
 *                We add this function to force users to set rf cal sector, since
 *                we don't know which sector is free in user's application.
 *                sector map for last several sectors : ABCCC
 *                A : rf cal
 *                B : rf init data
 *                C : sdk parameters
 * Parameters   : none
 * Returns      : rf cal sector
*******************************************************************************/
uint32 user_rf_cal_sector_set(void)
{
    flash_size_map size_map = system_get_flash_size_map();
    uint32 rf_cal_sec = 0;

    switch (size_map) {
        case FLASH_SIZE_4M_MAP_256_256:
            rf_cal_sec = 128 - 5;
            break;

        case FLASH_SIZE_8M_MAP_512_512:
            rf_cal_sec = 256 - 5;
            break;

        case FLASH_SIZE_16M_MAP_512_512:
        case FLASH_SIZE_16M_MAP_1024_1024:
            rf_cal_sec = 512 - 5;
            break;

        case FLASH_SIZE_32M_MAP_512_512:
        case FLASH_SIZE_32M_MAP_1024_1024:
            rf_cal_sec = 1024 - 5;
            break;
        case FLASH_SIZE_64M_MAP_1024_1024:
            rf_cal_sec = 2048 - 5;
            break;
        case FLASH_SIZE_128M_MAP_1024_1024:
            rf_cal_sec = 4096 - 5;
            break;
        default:
            rf_cal_sec = 0;
            break;
    }

    return rf_cal_sec;
}

void ICACHE_FLASH_ATTR wifi_event_handler_cb(System_Event_t *event) {
    static int running = 0;

    if (event->event_id == EVENT_STAMODE_GOT_IP) {
        if (running) {
			mdns_update_ip(mdns, event->event_info.got_ip.ip);
            return;
        }
        running = 1;

		mdns = mdns_create("esp8266");
		mdns_update_ip(mdns, event->event_info.got_ip.ip);    
		startup(NULL);
    }
}


/******************************************************************************
 * FunctionName : user_init
 * Description  : entry of user application, init user function here
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
void user_init(void) {
    printf("SDK version:%s\n", system_get_sdk_version());
    wifi_set_event_handler_cb(wifi_event_handler_cb);

    // wifi_set_opmode(STATION_MODE); 
    // struct station_config *sconfig = (struct station_config *)zalloc(sizeof(struct station_config));
    // sprintf(sconfig->ssid, "Kampftoast");
    // sprintf(sconfig->password, "dunkelstern738");
    // wifi_station_set_config(sconfig);
    // free(sconfig);
    // wifi_station_connect();
}

void startup(void *userData) {
	mdnsService *service = mdns_create_service("_workstation", mdnsProtocolUDP, 5353);
    mdns_service_add_txt(service, "bla", "blubb");
	mdns_add_service(mdns, service);

	mdns_start(mdns);
}

