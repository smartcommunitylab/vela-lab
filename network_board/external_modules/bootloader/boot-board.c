//note: this is a very dirty implementation to allow turning on leds from the various modules of the bootloader

#include "boot-board.h"
#include "dev/cc26xx-uart.h"
#include <stdint.h>

/* All interrupt masks */
#define CC26XX_UART_INTERRUPT_ALL (UART_INT_OE | UART_INT_BE | UART_INT_PE | \
                                   UART_INT_FE | UART_INT_RT | UART_INT_TX | \
                                   UART_INT_RX | UART_INT_CTS)

static void init_uart(void);

static void
disable_interrupts(void)
{
  /* Acknowledge UART interrupts */
  ti_lib_int_disable(INT_UART0_COMB);

  /* Disable all UART module interrupts */
  ti_lib_uart_int_disable(UART0_BASE, CC26XX_UART_INTERRUPT_ALL);

  /* Clear all UART interrupts */
  ti_lib_uart_int_clear(UART0_BASE, CC26XX_UART_INTERRUPT_ALL);
}

void boot_board_init(void){
  ti_lib_ioc_pin_type_gpio_output(LED_RED_ID);
  ti_lib_gpio_clear_dio(LED_RED_ID);
  ti_lib_ioc_pin_type_gpio_output(LED_GREEN_ID);
  ti_lib_gpio_clear_dio(LED_GREEN_ID);

  init_uart();
}

void boot_board_set_led(uint32_t dioNumber){
    ti_lib_gpio_set_dio(dioNumber);
}

void boot_board_clear_led(uint32_t dioNumber){
    ti_lib_gpio_clear_dio(dioNumber);
}

void boot_board_toggle_led(uint32_t dioNumber){
    ti_lib_gpio_toggle_dio(dioNumber);
}



void sendCharAsHexString(uint8_t c){
  char hc = TO_HEX((c>>4)&0x0F);
  boot_board_write_uart(&hc, 1);
  hc = TO_HEX(c & 0x0F) ;
  boot_board_write_uart(&hc, 1);
}

uint8_t boot_board_write_uart(char *bytes, uint32_t len){
    for(uint32_t i=0;i<len;i++){
        ti_lib_uart_char_put(UART0_BASE, bytes[i]);
    }
    return 0;
}

void boot_board_print_uin32_t_hex(uint32_t number){
  sendCharAsHexString((char)(0xFF&(number>>24)));
  sendCharAsHexString((char)(0xFF&(number>>16)));
  sendCharAsHexString((char)(0xFF&(number>>8)));
  sendCharAsHexString((char)(0xFF&(number)));
}

void boot_board_print_uin16_t_hex(uint16_t number){
  sendCharAsHexString((char)(0xFF&(number>>8)));
  sendCharAsHexString((char)(0xFF&(number)));
}

void boot_board_print_uin8_t_hex(uint8_t number){
  sendCharAsHexString((char)(0xFF&(number)));
}

void init_uart(void)
{
#if 0
/*
  ti_lib_prcm_power_domain_on(PRCM_DOMAIN_SERIAL);
  while(ti_lib_prcm_power_domain_status(PRCM_DOMAIN_SERIAL)
        != PRCM_DOMAIN_POWER_ON);

  ti_lib_prcm_peripheral_run_enable(PRCM_PERIPH_UART0);
  ti_lib_prcm_load_set();
  while(!ti_lib_prcm_load_get());

  disable_interrupts();

  uint32_t ctl_val = UART_CTL_UARTEN | UART_CTL_TXE;

  ti_lib_ioc_pin_type_gpio_output(BOARD_IOID_UART_TX);
  ti_lib_gpio_set_dio(BOARD_IOID_UART_TX);


  ti_lib_ioc_pin_type_uart(UART0_BASE, BOARD_IOID_UART_RX, BOARD_IOID_UART_TX,
                           BOARD_IOID_UART_CTS, BOARD_IOID_UART_RTS);

  ti_lib_uart_config_set_exp_clk(UART0_BASE, ti_lib_sys_ctrl_clock_get(),
                                 CC26XX_UART_CONF_BAUD_RATE,
                                 (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE |
                                  UART_CONFIG_PAR_NONE));

  HWREG(UART0_BASE + UART_O_CTL) = ctl_val;

  ti_lib_uart_int_clear(UART0_BASE, CC26XX_UART_INTERRUPT_ALL);
*/
#else

  uint32_t ctl_val = UART_CTL_UARTEN | UART_CTL_TXE;

  //uint8_t interrupts_disabled;
  //interrupts_disabled=ti_lib_int_master_disable();

  /* Make sure the peripheral is disabled */

  //disable_interrupts();

  /* Power on the SERIAL PD */
  ti_lib_prcm_power_domain_on(PRCM_DOMAIN_SERIAL);
  while(ti_lib_prcm_power_domain_status(PRCM_DOMAIN_SERIAL)
        != PRCM_DOMAIN_POWER_ON);

  /* Enable UART clock in active mode */
  ti_lib_prcm_peripheral_run_enable(PRCM_PERIPH_UART0);
  ti_lib_prcm_load_set();
  while(!ti_lib_prcm_load_get());

  /*
   * Make sure the TX pin is output / high before assigning it to UART control
   * to avoid falling edge glitches
   */
  ti_lib_ioc_pin_type_gpio_output(BOARD_IOID_UART_TX);
  ti_lib_gpio_set_dio(BOARD_IOID_UART_TX);

  /*
   * Map UART signals to the correct GPIO pins and configure them as
   * hardware controlled.
   */
  ti_lib_ioc_pin_type_uart(UART0_BASE, BOARD_IOID_UART_RX, BOARD_IOID_UART_TX,
                           BOARD_IOID_UART_CTS, BOARD_IOID_UART_RTS);

  /* Configure the UART for 115,200, 8-N-1 operation. */
  ti_lib_uart_config_set_exp_clk(UART0_BASE, ti_lib_sys_ctrl_clock_get(),
                                 CC26XX_UART_CONF_BAUD_RATE,
                                 (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE |
                                  UART_CONFIG_PAR_NONE));

  /* Enable TX and the UART. */
  HWREG(UART0_BASE + UART_O_CTL) = ctl_val;
  ti_lib_uart_enable(UART0_BASE);

  //if(!interrupts_disabled) {
  //  ti_lib_int_master_enable();
  //}
#endif
}

void deinit_uart(){

    while(ti_lib_uart_busy(UART0_BASE));

    ti_lib_uart_disable(UART0_BASE);

    disable_interrupts();

    ti_lib_prcm_peripheral_run_disable(PRCM_PERIPH_UART0);
    ti_lib_prcm_load_set();
    while(!ti_lib_prcm_load_get());

    /* Power on the SERIAL PD */
    ti_lib_prcm_power_domain_off(PRCM_DOMAIN_SERIAL);
    while(ti_lib_prcm_power_domain_status(PRCM_DOMAIN_SERIAL)
          != PRCM_DOMAIN_POWER_OFF);


}

void boot_board_deinit(void){
    deinit_uart();
}

