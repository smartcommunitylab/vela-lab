/**
 * sink_process.c - this will start all the processes needed at the vela sink node
   1. SLIP
   2. start unicast receiver
   3. start trickle sender

channel check interval
*
**/

#include "contiki.h"
#include "sys/etimer.h"
#include <stdio.h>
#include <string.h>
#include "sink_receiver.h"


PROCESS(sink_process, "sink starter process");


AUTOSTART_PROCESSES (&sink_process);



/*---------------------------------------------------------------------------*/
PROCESS_THREAD(sink_process, ev, data)
{
  PROCESS_BEGIN();
  printf("sink: started\n");

  // initialize and start the other threads
  sink_receiver_init();

  // do something, probably related to the watchdog
  
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/


