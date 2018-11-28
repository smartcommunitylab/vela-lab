#ifndef VELA_SENDER_H
#define VELA_SENDER_H

#include "constraints.h"

PROCESS_NAME (vela_sender_process);
PROCESS_NAME (cc2650_uart_process);

#define MAX_RPL_SEND_SIZE 			5 * SINGLE_NODE_REPORT_SIZE  // needs to be a multiple of SINGLE_NODE_REPORT_SIZE
#define TIME_BETWEEN_SENDS			5 * CLOCK_SECOND
//#define HEADER_SIZE                 4
#define MAX_PACKET_BUF              54      // biggest packet size before fragmentation occurs
#define KEEP_ALIVE_PORT 30000

// the event to be raised between the uart and the sender
process_event_t event_buffer_empty;
//event to send keep alive packet
process_event_t keep_alive_;

process_event_t event_bat_data_ready;

process_event_t event_new_keep_alive_time;

// to initialize the fake_uart
void vela_sender_init();


#endif 
