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
#include "constraints.h"
#include "servreg-hack.h"
#include "net/rpl/rpl.h"
#include <stdio.h>
#include <string.h>
#include "sink_receiver.h"

//static struct simple_udp_connection unicast_connection;
static data_t buffer;


static uint8_t childDataBuffer[500];
static uint8_t fakePacketCounter;


/*---------------------------------------------------------------------------*/
PROCESS(sink_receiver_process, "Vela sink receiver process");
/*---------------------------------------------------------------------------*/
void sink_receiver_init() {
  // FAKE: fill the buffer with the fake data that is fake-received
  buffer.data_len = 500;
  buffer.p_data = childDataBuffer;
  int i;
  uint8_t j=0;
  for (i=0;i<500;i++) {
    buffer.p_data[i] = j++;
  }
  fakePacketCounter = 1;

  process_start(&sink_receiver_process, "Vela sink receiver process");
  return;
}
/*---------------------------------------------------------------------------*/
static void receiver( const uint8_t *data, uint16_t datalen) 
{


  // this is a fake receiver to test formatting in different ways

  // send the ID of the source node
  // printf(" %u ", sender_addr->u8[15]); // eventually this will get only the nodeID
  printf("%c",42); // FKAE senderID=42

  // send the packet number  -- 
  // if the packet count > 128, this is the last packet of a sequence.   
  uint8_t mask = 128;
  if (fakePacketCounter%2 == 0)  printf("%c",fakePacketCounter^mask); // FAKE: even packets pretend to be the last of a sequence
  else printf("%c",fakePacketCounter); // FAKE: odd  packets  pretend to be NOT the last of a sequence
  fakePacketCounter++;

  // send the length of the data
  // length --- sending a 16 bit length
  printf("%c%c",(uint8_t)datalen,(uint8_t)(datalen>>8));

  // print the data
  int i;
  for (i=0;i<datalen;i++) 
    printf("%c",(uint8_t)data[i]);
  printf("\n");

  /*  
  printf("Received from ");
  uip_debug_ipaddr_print(sender_addr);
  printf(" length=%d: count=%u: ", datalen, (uint8_t)data[0] ); //, (uint8_t)data[0]);//, (uint8_t)data[4], (uint8_t)data[5]);
  int i;
  for (i=1;i<3;i++) 
    printf("%x",(uint8_t)data[i]);
    printf("\n"); 
  */
}
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(sink_receiver_process, ev, data)
{
  //uip_ipaddr_t *ipaddr;

  PROCESS_BEGIN();
  static struct etimer periodic_timer;
  etimer_set(&periodic_timer, SEND_INTERVAL);

  while(1) {
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));
    etimer_reset(&periodic_timer);
    receiver(buffer.p_data, buffer.data_len);
  }
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
