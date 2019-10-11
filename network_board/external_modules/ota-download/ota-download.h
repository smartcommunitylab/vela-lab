/** @file   ota-download.h
 *  @brief  OTA Image Download Mechanism
 *  @author Mark Solters <msolters@gmail.com>
 */

#ifndef OTA_DOWNLOAD_H
#define OTA_DOWNLOAD_H

#include "ota.h"

#define OTA_BUFFER_SIZE 256 //must be the same of OTA_CHUNK_SIZE in sink_receiver.c and MAX_CHUNK_SIZE in main.py

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
