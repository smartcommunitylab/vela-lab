/**
 * vela_sender.c - this file drives the sending of RPL packets. The intent is
 * for it to be used from the UART by raising an event event_data_ready, then
 * the data will be sent
 * 
 **/
#include "contiki.h"
#include "sys/etimer.h"
#include "net/ip/uip.h"
#include "net/ipv6/uip-ds6.h"
#include "net/ip/uip-debug.h"
#include "sys/node-id.h"
#include "simple-udp.h"
#include "servreg-hack.h"
#include "vela_uart.h"
#include "vela_sender.h"
#include "constraints.h"
#include <stdio.h>


static struct simple_udp_connection unicast_connection;
static uint8_t message_number;


PROCESS(vela_sender_process, "vela sender process");
/*---------------------------------------------------------------------------*/
static void
receiver(struct simple_udp_connection *c,
         const uip_ipaddr_t *sender_addr,
         uint16_t sender_port,
         const uip_ipaddr_t *receiver_addr,
         uint16_t receiver_port,
         const uint8_t *data,
         uint16_t datalen)
{
  printf("Data received on port %d from port %d with length %d\n",
         receiver_port, sender_port, datalen);
}
/*---------------------------------------------------------------------------*/
static void
set_global_address(void)
{
  uip_ipaddr_t ipaddr;
  int i;
  uint8_t state;

  uip_ip6addr(&ipaddr, UIP_DS6_DEFAULT_PREFIX, 0, 0, 0, 0, 0, 0, 0);
  uip_ds6_set_addr_iid(&ipaddr, &uip_lladdr);
  uip_ds6_addr_add(&ipaddr, 0, ADDR_AUTOCONF);

  printf("IPv6 addresses: ");
  for(i = 0; i < UIP_DS6_ADDR_NB; i++) {
    state = uip_ds6_if.addr_list[i].state;
    if(uip_ds6_if.addr_list[i].isused &&
       (state == ADDR_TENTATIVE || state == ADDR_PREFERRED)) {
      uip_debug_ipaddr_print(&uip_ds6_if.addr_list[i].ipaddr);
      printf("\n");
    }
  }
}
/*---------------------------------------------------------------------------*/
void vela_sender_init() {
  // set some parameters locally

  printf("vela_sender: initializing \n");

  servreg_hack_init();

  set_global_address();

 simple_udp_register(&unicast_connection, UDP_PORT, NULL, UDP_PORT, receiver);

  // start this process
  process_start(&vela_sender_process, "vela sender process");
  return;

}

/*---------------------------------------------------------------------------*/
static void send_to_sink(uint8_t *buffer, int offset, int size, bool last) {
  uip_ipaddr_t *addr = servreg_hack_lookup(SERVICE_ID);
  if (++message_number == 128)  message_number = 1; 

  static char buf[MAX_RPL_SEND_SIZE+1]; // add one for the message counter

  // prepend the message counter
  buf[0] = message_number;

  uint8_t mask = 128;
  if (last) buf[0]=buf[0]^mask; // if this is the last of a packet sequence, flip most significant bit

  int i;
  for (i=1;i<size;i++) {
    buf[i] = buffer[offset+i-1];
  }

  simple_udp_sendto(&unicast_connection, buf, size+1, addr);
  printf("sending %d bytes %u:%u %u\n",size+1,(uint8_t)buf[0], (uint8_t)buf[1], (uint8_t)buf[2]);

}
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(vela_sender_process, ev, data) { 

  PROCESS_BEGIN();

  event_buffer_empty = process_alloc_event();

  uip_ipaddr_t *addr;

  printf("unicast: started\n");
  while (1)
    {
      // wait until we get a data_ready event
      PROCESS_WAIT_EVENT_UNTIL(ev == event_data_ready);
      printf("vela_sender: received an event\n");

      data_t* eventData = (data_t*) data;


     addr = servreg_hack_lookup(SERVICE_ID);

     if(addr != NULL && eventData->data_len > 0) 
       {

      int offset=0;
      int sizeToSend = eventData->data_len;
      while ( sizeToSend > 0 ) {

        if (sizeToSend > MAX_RPL_SEND_SIZE) {
          // I have more data to send that will fit in one packet
          send_to_sink(eventData->p_data, offset, MAX_RPL_SEND_SIZE, false);
          sizeToSend -= MAX_RPL_SEND_SIZE; 
          offset += MAX_RPL_SEND_SIZE;
        } else {
          // this is my last packet
          send_to_sink(eventData->p_data, offset, sizeToSend, true);
          sizeToSend = 0;
          offset += sizeToSend;
        }
        
      } 

    } else {
      printf("Service %d not found\n", SERVICE_ID);
     }

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





