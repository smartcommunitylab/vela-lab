/**
 * uart.c - the intent of this file is to serve as stub for the main functionality of the UART code.  It needs to 
   1. initialize 
   2. periodically raise an even to the sender that data is ready
   3. receive an event that the buffer can be used 
   4. receive an event that configuration data has been received
 *
 */

#include "contiki.h"
#include "sys/etimer.h"
#include "vela_uart.h"
#include "vela_shared.h"
#include <stdio.h>
#include "vela_sender.h"

PROCESS(cc2650_uart_process, "cc2650 uart process STUB");

data_t buffer;

uint8_t childDataBuffer[540];
//int bufferSize;

/*---------------------------------------------------------------------------*/
void vela_uart_init() {
  // set some parameters locally

  printf("uart (fake): initializing \n");

  // for fake, fill the buffer with the fake data to be sent
  buffer.data_len = 171;
  buffer.p_data = childDataBuffer;
  int i;
  uint8_t j=0;
  for (i=0;i<540;i++) {
    buffer.p_data[i] = j++;
  }

  // start this process
  process_start(&cc2650_uart_process, "cc2650 uart process STUB");
  return;

}

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(cc2650_uart_process, ev, data) { 

  PROCESS_BEGIN();

  // initalize
  printf("uart (fake): started\n");

  // start the main processing
  static struct etimer periodic_timer;
  event_data_ready = process_alloc_event();
  etimer_set(&periodic_timer, SEND_INTERVAL);


  while (1) {

    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));
    etimer_reset(&periodic_timer);
    printf("uart: posting event_data_ready\n");
    process_post(&vela_sender_process, event_data_ready, &buffer);

  }


  PROCESS_END();
  
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/





