#ifndef VELA_SPI_H
#define VELA_SPI_H

#include "contiki.h"
#include "dev/cc26xx-uart.h"
#include "dev/serial-line.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "constraints.h"
#include "vela_sender.h"
#include "network_messages.h"
//#include "sequential_procedures.h"
#include "dev/leds.h"

#include "sys/ctimer.h"


#include "dev/spi.h"

#include "sys/log.h"

#define LOG_MODULE "vela_spi"
#define LOG_LEVEL LOG_LEVEL_INFO

#define NORDIC_WATCHDOG_ENABLED	1

#define MAX_PING_PAYLOAD 100
PROCESS_NAME (spi_process);

#define SEND_TIME 4 * CLOCK_SECOND
#define WAIT_RX 0.01 * CLOCK_SECOND
#define NB_REPORT_INTERVAL 15 * CLOCK_SECOND
/*Pin configurations ---------------------------------------------------------*/
#define SPI_PIN_SCK         IOID_18
#define SPI_PIN_MOSI        IOID_2
#define SPI_PIN_MISO        IOID_3
#define SPI_PIN_CS          IOID_19

#define SPI_CONTROLLER  SPI_CONTROLLER_SPI0

static const spi_device_t spi_dev = {
  .spi_controller = SPI_CONTROLLER,
  .pin_spi_sck = SPI_PIN_SCK,
  .pin_spi_miso = SPI_PIN_MISO,
  .pin_spi_mosi = SPI_PIN_MOSI,
  .pin_spi_cs = SPI_PIN_CS,
  .spi_bit_rate = 1000000,
  .spi_pha = 0,
  .spi_pol = 0
};

/*SPI commands --------------------------------------------------------------*/
#define ACK                 0x01
#define RESET               0x02
#define BT_SCAN             0x04
#define BT_ADV              0x05
#define REQ_BEACONS         0x86
#define BT_SCAN_WINDOW      0x07
#define BT_SCAN_INTERVAL    0x08

#define ON                     1
#define OFF                    0
/*---------------------------------------------------------------------------*/

// For compatibility with previously used uart protocol
#define APP_ACK_SUCCESS                         0
#define APP_ERROR_COMMAND_NOT_VALID             6

// the event to be raised between the uart and the sender
process_event_t event_data_ready;
process_event_t event_ping_requested;
process_event_t event_pong_received;
process_event_t event_nordic_message_received;
process_event_t turn_bt_on;
process_event_t turn_bt_off;
process_event_t turn_bt_on_w_params;
process_event_t turn_bt_on_def;
process_event_t reset_bt;
process_event_t turn_ble_tof_onoff;

process_event_t turn_bt_on_low;
process_event_t turn_bt_on_high;

typedef enum{
	start_of_data,
	more_data,
	end_of_data
} report_type_t;

#define MAX_MESH_PAYLOAD_SIZE	MAX_NUMBER_OF_BT_BEACONS*SINGLE_NODE_REPORT_SIZE

#define PING_PACKET_SIZE		5
#define BT_REPORT_BUFFER_SIZE 	MAX_MESH_PAYLOAD_SIZE

#define DEFAULT_ACTIVE_SCAN         1     //boolean
#define DEFAULT_SCAN_INTERVAL       3520       /**< Scan interval between 0x0004 and 0x4000 in 0.625 ms units (2.5 ms to 10.24 s). */
#define DEFAULT_SCAN_WINDOW         1920     /**< Scan window between 0x0004 and 0x4000 in 0.625 ms units (2.5 ms to 10.24 s). */
#define DEFAULT_TIMEOUT             0
#define DEFAULT_REPORT_TIMEOUT_MS   60000
#define UART_ACK_DELAY_US           2000

/** Converts a macro argument into a character constant.
 */
#define STRINGIFY(val)  #val

#if NORDIC_WATCHDOG_ENABLED
void nordic_watchdog_handler(void *ptr);
#endif

uint32_t report_ready(data_t *p_data);
void reset_nordic(void);
uint8_t send_ready(void);

// -----------------SPI-------------------------

void vela_spi_process_init();
void spi_init();
void spi_bt_scan_on(void);
void spi_bt_scan_off(void);


#endif 