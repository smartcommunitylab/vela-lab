/**
 * fake_uart.c - the intent of this file is to serve as stub for the main functionality of the UART code.  It needs to 
   1. initialize 
   2. periodically raise an even to the sender that data is ready
   3. receive an event that the buffer can be used 
   4. receive an event that configuration data has been received
 *
 */

#include "contiki.h"
#include "sys/etimer.h"
#include "fake_uart.h"
#include <stdio.h>
#include "vela_sender.h"

PROCESS(fake_uart_process, "fake uart receiver process");

data_t buffer;

uint8_t childDataBuffer[500];
//int bufferSize;

/*---------------------------------------------------------------------------*/
void fake_uart_init() {
  // set some parameters locally

  printf("uart: initializing \n");

  // for fake, fill the buffer with the fake data to be sent
  buffer.data_len = 500;
  buffer.p_data = childDataBuffer;
  int i;
  uint8_t j=0;
  for (i=0;i<500;i++) {
    buffer.p_data[i] = j++;
  }

  // start this process
  process_start(&fake_uart_process, "fake uart process");
  return;

}

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(fake_uart_process, ev, data) { 

  PROCESS_BEGIN();

  // initalize
  printf("uart: started\n");

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





