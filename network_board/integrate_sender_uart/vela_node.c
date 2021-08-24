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
#include "vela_spi.h"
#include "vela_sender.h"
#include "vela_node.h"

#include "sys/log.h"

#include "dev/leds.h"
#include "net/mac/tsch/tsch.h"
//#define LOG_MODULE "vela_node"
// #define LOG_LEVEL LOG_LEVEL_WARN

PROCESS(vela_node_process, "main starter process for non-sink");

AUTOSTART_PROCESSES (&vela_node_process);

/*---------------------------------------------------------------------------*/
//TODO delete this process and merge the initilization somewhere else
PROCESS_THREAD(vela_node_process, ev, data)
{
  
  PROCESS_BEGIN();
  //tsch_set_coordinator(1);
  //NETSTACK_MAC.on();

  //LOG_INFO("main: started\n");

  // initialize and start the other threads
  vela_spi_process_init();
  //LOG_INFO("SPI initialized\n");

  vela_sender_init();

  while (1)
    {
      PROCESS_WAIT_EVENT() ;
    }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/


