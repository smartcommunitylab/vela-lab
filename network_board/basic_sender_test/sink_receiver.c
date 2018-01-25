/*
 * Copyright (c) 2011, Swedish Institute of Computer Science.
 * All rights reserved.
 * This file is part of the Contiki operating system.
 */

#include "contiki.h"
#include "lib/random.h"
#include "sys/ctimer.h"
#include "sys/etimer.h"
#include "net/ip/uip.h"
#include "net/ipv6/uip-ds6.h"
#include "net/ip/uip-debug.h"
#include "simple-udp.h"
#include "servreg-hack.h"
#include "net/rpl/rpl.h"
#include <stdio.h>
#include <string.h>
#include "sink_receiver.h"
#include "dev/leds.h"
#include "dev/cc26xx-uart.h"

static struct simple_udp_connection unicast_connection;
static uint8_t from_two;
static uint8_t from_three;
static uint8_t from_five;

/*---------------------------------------------------------------------------*/
PROCESS(sink_receiver_process, "Vela sink receiver process");
/*---------------------------------------------------------------------------*/
void sink_receiver_init() {
  process_start(&sink_receiver_process, "Vela sink receiver process");
  from_two = 0;
  from_three = 0;
  from_five = 0;
  return;
}
/*---------------------------------------------------------------------------*/
static void
receiver(struct simple_udp_connection *c,
         const uip_ipaddr_t *sender_addr,  // 4 byte address
         uint16_t sender_port,
         const uip_ipaddr_t *receiver_addr,
         uint16_t receiver_port,
         const uint8_t *data,
         uint16_t datalen)
{
  leds_toggle(LEDS_GREEN);
	
  // send a START  sequence
  printf("%c%c%c%c",42, 42, 42, 42);

  // send the ID of the source node
  printf("%c", sender_addr->u8[15]); // this gets only the nodeID

  // send the packet number - 8 bts, 1st bit is 1 if this is the "last" of a sequence of packets
  printf("%c", (uint8_t)data[0]);

  // send the length of the data - 16 bits
  printf("%c",(uint8_t)(datalen));
  printf("%c",(uint8_t)(datalen>>8));
    

  // send the actual data.  datalen is actually the real data lenght + 1, where the 1 is the packet number. We still send this because of thbring e \n
  int i;
  for (i=1;i<datalen;i++) 
    printf("%c",(uint8_t)data[i]);
  printf("\n");


  /* HUMAN READABLE */
  /*    // send a START  sequence
  printf("%c%c%c%c",42, 42, 42, 42);

  // send the ID of the source node
  printf("nodeid.pkt=%u.", sender_addr->u8[15]); // this will get only the nodeID

  // send the packet number - 8 bts, 1st bit is 1 if this is the "last" of a sequence of packets
  uint8_t n = (uint8_t)data[0];  
  if (n>128) n-=128;
  printf("%u ", n);

  // send the length of the data - 16 bits
  printf("len=%d ",datalen);

  // send the actual data.  datalen is actually the real data lenght + 1, where the 1 is the packet number. We still send this because of the \n
  printf("data= ");
  int i;
  for (i=1;i<datalen;i++) 
    printf("[%d]=%u ",i,(uint8_t)data[i]);
  //  printf("last=%u ",(uint8_t)data[datalen]);
  printf("\n");
*/


  /*
  //// PRETTY PRINT SINK RECEIVED DATA
  ///////////////checking that no packets are lost
  uint8_t packet_num = (uint8_t)data[0]; 
  if (packet_num > 128) packet_num -= 128;
  uint8_t from = sender_addr->u8[15];
  if (from == 2) {
    if (from_two == 128) from_two = 1;
    if (packet_num > from_two) printf("*************** 2 LOST PACKET last=%u, this=%u \n",from_two, packet_num);
    from_two =  packet_num+1;
  } if (from == 3) {
    if (from_three == 128) from_three = 1;
    if (packet_num > from_three) printf("*************** 3 LOST PACKET last=%u, this=%u \n",from_three, packet_num);
    from_three =  packet_num+1;
  }if (from == 4) {
    if (from_five == 128) from_five = 1;
    if (packet_num > from_five) printf("*************** 4 LOST PACKET last=%u, this=%u \n",from_five, packet_num);
    from_five =  packet_num+1;
  }
  
  printf("Received from ");
  
  printf(" %u ", sender_addr->u8[15]);
  //uip_debug_ipaddr_print(sender_addr);
  if ((uint8_t)data[0]<=128)
    printf(" length=%d: count=%u: ", datalen, (uint8_t)data[0] ); //, (uint8_t)data[0]);//, (uint8_t)data[4], (uint8_t)data[5]);
  else
    printf(" length=%d: count=%u: ", datalen, (uint8_t)data[0] -128); //, (uint8_t)data[0]);//, (uint8_t)data[4], (uint8_t)data[5]);
  //  int i;
  //for (i=1;i<3;i++) 
  //printf("%x",(uint8_t)data[i]); 
  printf("\n");  
*/
}
/*---------------------------------------------------------------------------*/
static uip_ipaddr_t *
set_global_address(void)
{
  static uip_ipaddr_t ipaddr;
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

  return &ipaddr;
}
/*---------------------------------------------------------------------------*/
static void
create_rpl_dag(uip_ipaddr_t *ipaddr)
{
  struct uip_ds6_addr *root_if;

  root_if = uip_ds6_addr_lookup(ipaddr);
  if(root_if != NULL) {
    rpl_dag_t *dag;
    uip_ipaddr_t prefix;
    
    rpl_set_root(RPL_DEFAULT_INSTANCE, ipaddr);
    dag = rpl_get_any_dag();
    uip_ip6addr(&prefix, UIP_DS6_DEFAULT_PREFIX, 0, 0, 0, 0, 0, 0, 0);
    rpl_set_prefix(dag, &prefix, 64);
    PRINTF("created a new RPL dag\n");
  } else {
    PRINTF("failed to create a new RPL DAG\n");
  }
}
/*---------------------------------------------------------------------------*/
static int
dummy_uart_receiver(unsigned char c) //this function will be useful in the future. For now setting the input function disables the power saving of the uart peripheral.
{
	(void) c;
	leds_toggle(LEDS_GREEN);
	return 0;
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(sink_receiver_process, ev, data)
{
  uip_ipaddr_t *ipaddr;

  PROCESS_BEGIN();

  cc26xx_uart_set_input(dummy_uart_receiver);

  servreg_hack_init();

  ipaddr = set_global_address();

  create_rpl_dag(ipaddr);

  servreg_hack_register(SERVICE_ID, ipaddr);

  simple_udp_register(&unicast_connection, UDP_PORT, NULL, UDP_PORT, receiver);

  while(1) {
    PROCESS_WAIT_EVENT();
  }
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
