#ifndef BOOT_BOARD_H_
#define BOOT_BOARD_H_

//note: this is a very dirty implementation to allow turning on leds from the various modules of the bootloader

#include "ti-lib.h"
#include <stdio.h>

#define TO_HEX(half_byte_data) (((half_byte_data) <=  9 ) ? ((half_byte_data) + '0') : ((half_byte_data) + 'A' - 10)) //NOTE THAT THIS PROPERLY WORKS ONLY ON 4 BITS!!!

#define LED_RED_ID      IOID_6
#define LED_GREEN_ID    IOID_7

#define BOARD_IOID_UART_RX        IOID_2
#define BOARD_IOID_UART_TX        IOID_3
#define BOARD_IOID_UART_RTS       IOID_18
#define BOARD_IOID_UART_CTS       IOID_19
#define BOARD_UART_RX             (1 << BOARD_IOID_UART_RX)
#define BOARD_UART_TX             (1 << BOARD_IOID_UART_TX)
#define BOARD_UART_RTS            (1 << BOARD_IOID_UART_RTS)
#define BOARD_UART_CTS            (1 << BOARD_IOID_UART_CTS)

extern char txt_buff[];
extern uint32_t txt_len;

void boot_board_init(void);

void boot_board_set_led(uint32_t dioNumber);
void boot_board_clear_led(uint32_t dioNumber);
void boot_board_toggle_led(uint32_t dioNumber);

uint8_t boot_board_write_uart(char *bytes, uint32_t len);
void boot_board_print_uin32_t_hex(uint32_t number);
void boot_board_print_uin16_t_hex(uint16_t number);
void boot_board_print_uin8_t_hex(uint8_t number);

void boot_board_deinit();

#endif //BOOT_BOARD_H_
