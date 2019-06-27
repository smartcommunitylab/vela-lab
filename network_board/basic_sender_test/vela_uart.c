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
#include "constraints.h"
#include <stdio.h>
#include "vela_sender.h"
#include "project-conf.h"
#include "sys/clock.h"
#include "dev/cc26xx-uart.h"

PROCESS(cc2650_uart_process, "cc2650 uart process STUB");


//fake fixed value for testing
#define FAKE_PACKET_SIZE 100 // fixed at 200 bytes for now
static uint16_t packetSize = FAKE_PACKET_SIZE;


/*---------------------------------------------------------------------------*/
void vela_uart_init() {
  // set some parameters locally

  printf("uart (fake): initializing \n");

  // start this process
  process_start(&cc2650_uart_process, "cc2650 uart process STUB");
  return;

}

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(cc2650_uart_process, ev, data) {

  static struct etimer uart_new_data;

  PROCESS_BEGIN();

  static data_t buffer;
  static uint8_t childDataBuffer[310];
  // for fake, fill the buffer with the fake data to be sent
  buffer.p_data = childDataBuffer;

  // initalize
  cc26xx_uart_set_input(NULL); //not striclty required

  printf("uart (fake): started\n");


  // start the main processing
  event_data_ready = process_alloc_event();
  buffer.p_data[0] = packetSize >> 8;
  buffer.p_data[1] = packetSize;

  uint8_t j = 0;
  uint8_t k = 0;
  for (int i = 0; i < packetSize; i++) {
    childDataBuffer[2 + i] = j;
    k++;
    if (k == 9) {
      j++;
      k = 0;
    }
  }


  etimer_set(&uart_new_data, 15*CLOCK_SECOND);
  PROCESS_WAIT_UNTIL(etimer_expired(&uart_new_data));
  while (1) {
    printf("UART TIMER FIRED\n");
    //if (etimer_expired(&uart_new_data)) {
      buffer.data_len = packetSize+2;
      printf("uart: posting event_data_ready\n");
      //printf("p1 %ux p2 %ux\n", buffer.p_data[0],  buffer.p_data[1]);
      process_post(&vela_sender_process, event_data_ready, &buffer);
      etimer_set(&uart_new_data, 15*CLOCK_SECOND);
        PROCESS_WAIT_UNTIL(etimer_expired(&uart_new_data));
    //}
    //PROCESS_YIELD();
  }
  PROCESS_END();

}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
