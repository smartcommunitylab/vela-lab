#ifndef VELA_UART_H
#define VELA_UART_H

#include "contiki.h"
#include "dev/cc26xx-uart.h"
#include "dev/serial-line.h"
#include "dev/spi.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "uart_util.h"
#include "constraints.h"
#include "vela_sender.h"
#include "network_messages.h"
#include "sequential_procedures.h"

// MACRO
#define MAX_PING_PAYLOAD 100
#define SPI_CONTROLLER  SPI_CONTROLLER_SPI0

// SPI PIN CONFIGURATION
#define SPI_PIN_SCK         IOID_18
#define SPI_PIN_MOSI        IOID_2
#define SPI_PIN_MISO        IOID_3
#define SPI_PIN_CS          IOID_19

//PROCESS_NAME (vela_sender_process);
PROCESS_NAME (cc2650_uart_process);



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
// to initialize the fake_uart
void vela_uart_init(void);

void spi_init(void);

void reset_nordic(void);

#endif 
