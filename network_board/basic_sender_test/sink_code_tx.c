/**
 * sink_code_tx.c - the intent of this file is to serve as stub for the main functionality of the OTA code transmission code.  It needs to
 1. initialize
 2. infrequently raise an even to the sender that code is ready
 *
 */

#include "contiki.h"
#include "contiki-net.h"
#include "sys/etimer.h"
#include "sink_code_tx.h"
#include "constraints.h"
#include "network_messages.h"
#include <stdio.h>
#include "sink_receiver.h"
#include "project-conf.h"
#include "sys/clock.h"
//#include "dev/cc26xx-uart.h" // REMOVED FOR COOJA

PROCESS(code_tx_process, "cc2650 code_tx process STUB");


//fake fixed value for testing
#define FAKE_CODE_SIZE 1024 


/*---------------------------------------------------------------------------*/
void sink_code_tx_init() {
  // set some parameters locally

  printf("sink_code_tx (fake): initializing \n");

  // start this process
  process_start(&code_tx_process, "sink code transmission  process STUB");
  return;

}

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(code_tx_process, ev, data) {

  static struct etimer code_new_tx;

  PROCESS_BEGIN();

  static codeChunk_t chunk;
  static uint8_t chunkBuffer[1024];
  // for fake, fill the buffer with the fake data to be sent
  chunk.p_data = chunkBuffer;
  chunk.data_len = 1024;
  chunk.destination = NULL;
  chunk.chunkID = 0;

  // initalize

  printf("sink code transmitter (fake): started\n");


  // start the main processing
  event_code_ready = process_alloc_event();

  for (int i=0; i<1024; i++) {
    chunkBuffer[i] = (uint8_t)i;
  }


  etimer_set(&code_new_tx, 60*CLOCK_SECOND);
  PROCESS_WAIT_UNTIL(etimer_expired(&code_new_tx));
  while (1) {
    chunk.chunkID++;
    printf("SENDING CODE TIMER FIRED, sending chunk id=%u\n",chunk.chunkID);

    // TODO  for now, this is commented out so that we NEVER send data.
    // Eventually, this needs to be the code that sends chunks of code
    //process_post(&sink_receiver_process, event_code_ready, &chunk);

    
    etimer_set(&code_new_tx, 60*CLOCK_SECOND); // TODO -- this was for testing-only, and by resulted in sending code chunks every 60s.  Eventually this should not be timer-driven, but instead driven by the process that receives the code from the gateway
    PROCESS_WAIT_UNTIL(etimer_expired(&code_new_tx));
  }
  PROCESS_END();

}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
