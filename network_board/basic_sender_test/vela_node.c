/**
 * vela_node.c - this will start all the processes needed at a non-sink node
   1. start uart
   2. start unicast sender
   3. start trickle receiver

channel check interval
*
**/

#include "contiki.h"
#include "sys/etimer.h"
#include <stdio.h>
#include <string.h>
#include "fake_uart.h"
#include "vela_sender.h"


PROCESS(vela_node_process, "main starter process for non-sink");


AUTOSTART_PROCESSES (&vela_node_process);



/*---------------------------------------------------------------------------*/
PROCESS_THREAD(vela_node_process, ev, data)
{
  PROCESS_BEGIN();
  printf("main: started\n");

  // initialize and start the other threads
  fake_uart_init();
  vela_sender_init();

  // do something, probably related to the watchdog
  
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/


