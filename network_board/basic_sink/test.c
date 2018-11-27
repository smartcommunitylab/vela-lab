#include "contiki.h"
#include "dev/serial-line.h"
#include <stdio.h>
 
 PROCESS(test_serial, "Serial line test process");
 AUTOSTART_PROCESSES(&test_serial);
 
 PROCESS_THREAD(test_serial, ev, data)
 {
   PROCESS_BEGIN();
 
   for(;;) {
     PROCESS_YIELD();
     if(ev == serial_line_event_message) {
       printf("received line: %s\n", (char *)data);
     }
   }
   PROCESS_END();
 }
