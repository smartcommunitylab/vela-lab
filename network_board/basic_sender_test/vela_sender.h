#ifndef VELA_SENDER_H
#define VELA_SENDER_H

#include "constraints.h"

PROCESS_NAME (vela_sender_process);

#define MAX_RPL_SEND_SIZE 			15*SINGLE_NODE_REPORT_SIZE  // needs to be a multiple of SINGLE_NODE_REPORT_SIZE
#define TIME_BETWEEN_SENDS			CLOCK_SECOND*5

// the event to be raised between the uart and the sender
process_event_t event_buffer_empty;


// to initialize the fake_uart
void vela_sender_init();

#endif 
