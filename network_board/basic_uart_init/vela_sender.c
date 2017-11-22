/**
 * vela_sender.c - this file drives the sending of RPL packets. The intent is
 * for it to be used from the UART by raising an event event_data_ready, then
 * the data will be sent
 * 
 **/
#include "contiki.h"
//#include "net/ip/uip.h"
//#include "net/ipv6/uip-ds6.h"
//#include "net/ip/uip-debug.h"
#include "vela_uart.h"
#include "vela_shared.h"
#include "vela_sender.h"



PROCESS(vela_sender_process, "vela sender (fake) process");
/*---------------------------------------------------------------------------*/
void vela_sender_init() {
  // set some parameters locally

  //printf("vela_sender: initializing \n");


  // start this process
  process_start(&vela_sender_process, "vela sender (fake) process");
  return;

}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(vela_sender_process, ev, data) { 

  PROCESS_BEGIN();

  //printf("unicast (fake): started\n");
  while (1)
    {
      // wait until we get a data_ready event
      PROCESS_WAIT_EVENT_UNTIL(ev == event_data_ready);
      //printf("vela_sender: received an event\n");
    }

  PROCESS_END();
  
}

/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/





