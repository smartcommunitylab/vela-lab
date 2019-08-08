/** @file   ota-download.h
 *  @brief  OTA Image Download Mechanism
 *  @author Mark Solters <msolters@gmail.com>
 */

#ifndef OTA_DOWNLOAD_H
#define OTA_DOWNLOAD_H

#include "ota.h"
//#include "contiki-net.h"

//static uip_ipaddr_t ota_server_ipaddr;
//#define OTA_SERVER_IP() uip_ip6addr(&ota_server_ipaddr, 0xbbbb, 0, 0, 0, 0, 0, 0, 0x1)
//#define SERVER_EP "coap://[fe80::212:7402:0002:0202]" //TODO: make this not-hardcoded

/* OTA Download Thread */
//extern struct process* ota_download_th_p; // Pointer to OTA Download thread

#define OTA_BUFFER_SIZE 256

OTAMetadata_t new_firmware_metadata;
int active_ota_download_slot;
uint8_t ota_buffer[ OTA_BUFFER_SIZE ];
bool metadata_received;
uint32_t ota_bytes_received;
uint32_t ota_bytes_saved;
uint32_t ota_req_start;
uint32_t img_req_position;
bool ota_download_active;

uint8_t ota_download_firmware_subchunk_handler(const uint8_t *chunk, uint32_t len);
void ota_download_init(void);

#endif
