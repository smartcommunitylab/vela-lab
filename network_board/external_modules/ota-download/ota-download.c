/** @file   ota-download.c
 *  @brief  OTA Image Download Mechanism
 *  @author Mark Solters <msolters@gmail.com>. Porting to be more general by Davide Giovanelli davigiov88@gmail.com
 */

#include "ota-download.h"
#include "ti-lib.h"


#include "sys/log.h"
#define LOG_MODULE "ota-download"
#define LOG_LEVEL LOG_LEVEL_DBG


void sanity_check(void);

static void
reset_ota_buffer() {
  uint16_t n;
  for (n=0; n<OTA_BUFFER_SIZE; n++)
  {
    ota_buffer[ n ] = 0xff;
  }
}

static bool
is_download_completed() {
    if ( metadata_received ) {
       if(ota_bytes_received==new_firmware_metadata.size + OTA_METADATA_SPACE) {
           return true;
       }
    }
    return false;
}


void ota_download_verify_active_slot(void){
    int8_t ver_res=verify_ota_slot( active_ota_download_slot );
    if(ver_res < 0){
        LOG_ERR("Verification failed, error code: %d\n",ver_res);
    }
}

void sanity_check(void){
    //  (4) When the request is complete (possibly failed), check how much data
    //      we've received thus far.
  /*  if ( metadata_received ) {
        //  If we have metadata, we can determine what the download's total size
        //  should have been.
        if ( ota_bytes_received >= (OTA_METADATA_SPACE + new_firmware_metadata.size) ) {
            //  We've received all the bytes we need.  Leave the download loop.
            ota_download_active = false;
        } else {
            //  Oh no!  Our download got interrupted somehow!
            //  Rewind the download to the end of the last valid page.
            ota_bytes_saved = (ota_bytes_saved/FLASH_PAGE_SIZE) << 12;
            LOG_INFO("Erasing %#lx\n", (ota_bytes_saved + (ota_images[active_ota_download_slot-1] << 12)));
            while( erase_extflash_page( (ota_bytes_saved + (ota_images[active_ota_download_slot-1] << 12)) ) );
        }
    } else {
        ota_bytes_saved = 0; 
        LOG_INFO("Erasing %#lx\n", ota_bytes_saved);
        //  We need to redownload starting with the last page, so let's clear it first.
        while( erase_extflash_page( ota_images[active_ota_download_slot-1] << 12 ) );
    }
*/
    //if(!ota_download_active){ //ota_download_active goes false only when a new image has been correctly received
        //  (5) If we've made it this far, we need to do another CoAP request.
        //ota_bytes_received = ota_bytes_saved;
        //coap_request_count++;

        //  (5) Save the last ota_buffer!  This may not happen in the firmware_chunk_handler
        //      if the last ota_buffer wasn't totally full, as img_req_position
        //      will never increment to OTA_BUFFER_SIZE.
        
        while( store_firmware_data( ( ota_bytes_saved + (ota_images[active_ota_download_slot-1] << 12)), ota_buffer, OTA_BUFFER_SIZE ) );
        ota_bytes_saved += img_req_position;

        LOG_INFO("All the firmware is stored in the external flash now.\n");

        LOG_INFO("Verifying the OTA download");
        ota_download_verify_active_slot(); //there is some problem here. By running this the device reboots, but anything else keeps working. Than I just put it as the last thing to execute before the reboot, then if the reboot is because of the watchdog or because the next line I don't care.

        //  (6) Recompute the CRC16 algorithm over the received data.  This value is
        //      called the "CRC Shadow." If it doesn't match the CRC value in the
        //      metadata, our download got messed up somewhere along the way!
        //while( verify_ota_slot( active_ota_download_slot ) );
        //if(verify_ota_slot( active_ota_download_slot ) != 0){
        //    LOG_ERR("Could not verify the ota firmware.\n");
        //} 
        
        //  (7) Reboot! NO MORE AUTOMATIC. THE SINK WILL SEND A MESSAGE TO REBOOT THE FULL NETWORK MORE OR LESS AT THE SAME TIME
        //LOG_INFO("-----OTA download done, rebooting!-----\n");
        //ti_lib_sys_ctrl_system_reset();
    //}
}

/*******************************************************************************
 * @fn      firmware_chunk_handler
 *
 * @brief   Handle incoming data from the CoAP request.
 *          This mostly involves writing the data to the ota_buffer[] array.
 *          When ota_buffer is full, we copy it into external flash.
 *
 */
uint8_t
ota_download_firmware_subchunk_handler(const uint8_t *chunk, uint32_t len)
{
    ota_download_active = true;
    uint8_t ret=0;

    //  (2) Copy the CoAP payload into the ota_buffer
    ota_bytes_received += len;
    LOG_INFO("Received %u new bytes:  ", (unsigned int)len);

    while (len--) {
        ota_buffer[ img_req_position++ ] = *chunk++;
    }

    //  (3) Handle metadata-specific information
    if ( metadata_received ) {
        //  If we have metadata already, calculate how much of the download is done.
        int percent = 10000.0 * ((float)( ota_bytes_saved + img_req_position) / (float)(new_firmware_metadata.size + OTA_METADATA_SPACE));
        LOG_INFO_(" %u.%u%%\n", percent/100, (percent - ((percent/100)*100)));
    } else if ( img_req_position >= OTA_METADATA_LENGTH ) {
        LOG_INFO("\nDownload metadata acquired: ");
        //  If we don't have metadata yet, get it from the first page
        //  (1) Extract metadata from the ota_buffer
        memcpy( &new_firmware_metadata, ota_buffer, OTA_METADATA_LENGTH );
        //print_metadata( &new_firmware_metadata );
        metadata_received = true;

        LOG_INFO_("crc: 0x%04x size 0x%08x uuid 0x%08x version 0x%04x\n",(unsigned int)new_firmware_metadata.crc, (unsigned int)new_firmware_metadata.size, (unsigned int)new_firmware_metadata.uuid, (unsigned int)new_firmware_metadata.version);
        
        if(new_firmware_metadata.crc_shadow != 0){ //non preverified images should have crc_shadow initialized to 0
            LOG_WARN("CRC shadow is not zero. The OTA download might not be secure!");
        }
        //  (2)  to see if we have any OTA slots already containing
        //      firmware of the same version number as the metadata has.
        active_ota_download_slot = find_matching_ota_slot( new_firmware_metadata.version );
        if ( active_ota_download_slot == -1 ) {
            //  We don't already have a copy of this firmware version, let's download
            //  to an empty OTA slot!
            active_ota_download_slot = find_empty_ota_slot();
            if ( !active_ota_download_slot ) {
                active_ota_download_slot = 1;
            }
        }
        LOG_INFO("Downloading OTA update to OTA slot %u. The slot is being erased.\n", active_ota_download_slot);

        //  (3) Erase the destination OTA download slot
        while( erase_ota_image( active_ota_download_slot ) );

    }

    //  (4) Save the latest ota_buffer to flash if it's full.
    if ( img_req_position >= OTA_BUFFER_SIZE ) {
        while( store_firmware_data( ( ota_bytes_saved + (ota_images[active_ota_download_slot-1] << 12)), ota_buffer, OTA_BUFFER_SIZE ) );
        LOG_INFO("Internal OTA buffer offloaded to external flash at %#lx. Total bytes saved till now: %u\n", ( ota_bytes_saved + (ota_images[active_ota_download_slot-1] << 12)) , (unsigned int)(ota_bytes_saved+img_req_position));
        ota_bytes_saved += img_req_position;
        img_req_position = 0;
        ret=1;
        reset_ota_buffer();
    }

    if(is_download_completed()){
        LOG_INFO("Download completed\n");
        sanity_check();
        ota_download_active = true;
        metadata_received = false;
        ret=1;
    }

    return ret;

}

void ota_download_init(void){

    LOG_INFO("Initializing ota-download module.\n");
    //  (2) Initialize download parameters
    reset_ota_buffer();
    metadata_received = false;
    ota_download_active = false;
    ota_bytes_received = 0;
    ota_bytes_saved = 0;

    memset(&new_firmware_metadata,0x00,sizeof(OTAMetadata_t));

    //  (1) Reset data received counters
    img_req_position = 0;
    ota_req_start = ota_bytes_saved;

    //active_ota_download_slot = find_empty_ota_slot();
    //LOG_INFO("Erasing OTA slot: %u\n", active_ota_download_slot);
            //  (3) Erase the destination OTA download slot
    //while( erase_ota_image( active_ota_download_slot ) );
}


