#include <stdint.h>
#include <string.h>
#include "ti-lib.h"
#include "ext-flash.h"

#if defined(ENABLE_BOOTLOADER_VERBOSITY) && ENABLE_BOOTLOADER_VERBOSITY==1
#include "boot-board.h"
#endif

#include "ota.h"

static void
power_domains_on(void) {
  /* Turn on the PERIPH PD */
  ti_lib_prcm_power_domain_on(PRCM_DOMAIN_PERIPH);

  /* Wait for domains to power on */
  while((ti_lib_prcm_power_domain_status(PRCM_DOMAIN_PERIPH)
        != PRCM_DOMAIN_POWER_ON));
}

void
initialize_peripherals() {
  /* Disable global interrupts */
  bool int_disabled = ti_lib_int_master_disable();

  power_domains_on();

  /* Enable GPIO peripheral */
  ti_lib_prcm_peripheral_run_enable(PRCM_PERIPH_GPIO);

  /* Apply settings and wait for them to take effect */
  ti_lib_prcm_load_set();
  while(!ti_lib_prcm_load_get());

  /* Make sure the external flash is in the lower power mode */
  ext_flash_init(NULL);

  #if defined(ENABLE_BOOTLOADER_VERBOSITY) && ENABLE_BOOTLOADER_VERBOSITY==1
    boot_board_init();
  #endif

  /* Re-enable interrupt if initially enabled. */
  if(!int_disabled) {
    ti_lib_int_master_enable();
  }

}

#if defined(ENABLE_BOOTLOADER_VERBOSITY) && ENABLE_BOOTLOADER_VERBOSITY==1
char txt_buff[200];
uint32_t txt_len;
#endif


int
main(void)
{
  initialize_peripherals();

#if defined(ENABLE_BOOTLOADER_VERBOSITY) && ENABLE_BOOTLOADER_VERBOSITY==1
  txt_len=sprintf(txt_buff,"\n\n\n\nBootloader started!\n");
  boot_board_write_uart(txt_buff,txt_len);
  //boot_board_set_led(LED_RED_ID);
#endif

  #if CLEAR_OTA_SLOTS
  erase_ota_image( 1 );
  erase_ota_image( 2 );
  erase_ota_image( 3 );
  #endif

  #if BURN_GOLDEN_IMAGE
  backup_golden_image();
  #endif

  //  (1) Get the metadata of whatever firmware is currently installed
  OTAMetadata_t current_firmware;
  get_current_metadata( &current_firmware );

#if defined(ENABLE_BOOTLOADER_VERBOSITY) && ENABLE_BOOTLOADER_VERBOSITY==1
  txt_len=sprintf(txt_buff,"Installed firmware (on internal flash) metadata:\ncrc: 0x");
  boot_board_write_uart(txt_buff,txt_len);
  boot_board_print_uin16_t_hex(current_firmware.crc);

  txt_len=sprintf(txt_buff,"\nsize: 0x");
  boot_board_write_uart(txt_buff,txt_len);
  boot_board_print_uin32_t_hex(current_firmware.size);

  txt_len=sprintf(txt_buff,"\nuuid: 0x");
  boot_board_write_uart(txt_buff,txt_len);
  boot_board_print_uin32_t_hex(current_firmware.uuid);
  
  txt_len=sprintf(txt_buff,"\nversion: 0x");
  boot_board_write_uart(txt_buff,txt_len);
  boot_board_print_uin16_t_hex(current_firmware.version);
  
  txt_len=sprintf(txt_buff,"\nverifying it...\n");
  boot_board_write_uart(txt_buff,txt_len);
#endif

  //  (2) Verify the current firmware! (Recompute the CRC over the internal flash image)
  bool curr_fw_valid = verify_current_firmware( &current_firmware ) == 0;
  if(curr_fw_valid){
      txt_len=sprintf(txt_buff,"Current firmware (in the internal flash) IS valid!\n");
      boot_board_write_uart(txt_buff,txt_len);
  }else{
      txt_len=sprintf(txt_buff,"Current firmware (in the internal flash) IS NOT valid!\n");
      boot_board_write_uart(txt_buff,txt_len);
  }

  //  (3) Are there any newer firmware images in ext-flash?
  uint8_t newest_ota_slot = find_newest_ota_image();
  OTAMetadata_t newest_firmware;

#if defined(ENABLE_BOOTLOADER_VERBOSITY) && ENABLE_BOOTLOADER_VERBOSITY==1
  txt_len=sprintf(txt_buff,"\nNewest OTA firmware (in the external flash) is at slot: 0x");
  boot_board_write_uart(txt_buff,txt_len);
  boot_board_print_uin8_t_hex(newest_ota_slot);

  txt_len=sprintf(txt_buff,"\n");
  boot_board_write_uart(txt_buff,txt_len);
#endif

  while( get_ota_slot_metadata( newest_ota_slot, &newest_firmware ) );

#if defined(ENABLE_BOOTLOADER_VERBOSITY) && ENABLE_BOOTLOADER_VERBOSITY==1
  txt_len=sprintf(txt_buff,"Newest OTA firmware metadata:\ncrc: 0x");
  boot_board_write_uart(txt_buff,txt_len);
  boot_board_print_uin16_t_hex(newest_firmware.crc);

  txt_len=sprintf(txt_buff,"\ncrc shadow: 0x");
  boot_board_write_uart(txt_buff,txt_len);
  boot_board_print_uin16_t_hex(newest_firmware.crc_shadow);

  txt_len=sprintf(txt_buff,"\nsize: 0x");
  boot_board_write_uart(txt_buff,txt_len);
  boot_board_print_uin32_t_hex(newest_firmware.size);

  txt_len=sprintf(txt_buff,"\nuuid: 0x");
  boot_board_write_uart(txt_buff,txt_len);
  boot_board_print_uin32_t_hex(newest_firmware.uuid);
  
  txt_len=sprintf(txt_buff,"\nversion: 0x");
  boot_board_write_uart(txt_buff,txt_len);
  boot_board_print_uin16_t_hex(newest_firmware.version);
  
  txt_len=sprintf(txt_buff,"\n\n");
  boot_board_write_uart(txt_buff,txt_len);
#endif

  //  (4) Is the current image valid?
  if ( validate_ota_metadata( &current_firmware , INTERNAL_IMAGE_SLOT) ) {
    if ( ( newest_ota_slot > 0 ) && (newest_firmware.version > current_firmware.version) ) {
      //  If there's a newer firmware image than the current firmware, install
      //  the newer version!
#if defined(ENABLE_BOOTLOADER_VERBOSITY) && ENABLE_BOOTLOADER_VERBOSITY==1
      txt_len=sprintf(txt_buff,"There's a newer firmware image in the external memory, installing it...\n");
      boot_board_write_uart(txt_buff,txt_len);
#endif
      update_firmware( newest_ota_slot );

#if defined(ENABLE_BOOTLOADER_VERBOSITY) && ENABLE_BOOTLOADER_VERBOSITY==1
      txt_len=sprintf(txt_buff,"New firmware installed, rebooting with it..\n\n");
      boot_board_write_uart(txt_buff,txt_len);
#endif
      ti_lib_sys_ctrl_system_reset(); // reboot
    } else {

      //boot_board_clear_led(LED_GREEN_ID);
      //  If our image is valid, and there's nothing newer, then boot the firmware.
#if defined(ENABLE_BOOTLOADER_VERBOSITY) && ENABLE_BOOTLOADER_VERBOSITY==1
      txt_len=sprintf(txt_buff,"Internal image is valid, and there's nothing valid newer, then boot the firmware..\n\n");
      boot_board_write_uart(txt_buff,txt_len);
#endif

      jump_to_image( (CURRENT_FIRMWARE<<12) );
    }
  } else {
    //  If our image is not valid, install the newest valid image we have.
    //  Note: This can be the Golden Image, when newest_ota_slot = 0.
#if defined(ENABLE_BOOTLOADER_VERBOSITY) && ENABLE_BOOTLOADER_VERBOSITY==1
    txt_len=sprintf(txt_buff,"Internal image is not valid, installing the newest valid image we have which is slot: 0x");
    boot_board_write_uart(txt_buff,txt_len);
    boot_board_print_uin8_t_hex(newest_ota_slot);

    txt_len=sprintf(txt_buff,"\n");
    boot_board_write_uart(txt_buff,txt_len);
#endif
    
    update_firmware( newest_ota_slot );

#if defined(ENABLE_BOOTLOADER_VERBOSITY) && ENABLE_BOOTLOADER_VERBOSITY==1    
    txt_len=sprintf(txt_buff,"Firmware installed, rebooting with it..\n\n");
    boot_board_write_uart(txt_buff,txt_len);
#endif

    ti_lib_sys_ctrl_system_reset(); // reboot
  }

  //  main() *should* never return - we should have rebooted or branched
  //  to other code by now.
  return 0;
}
