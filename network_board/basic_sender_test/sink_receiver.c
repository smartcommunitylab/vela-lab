/*
 * Copyright (c) 2011, Swedish Institute of Computer Science.
 * All rights reserved.
 * This file is part of the Contiki operating system.
 */
#include <stdio.h>
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
#include <string.h>
#include "sink_receiver.h"
#include "dev/leds.h"
#include "dev/cc26xx-uart.h"
#include "net/ip/uip-udp-packet.h"
#include "lib/trickle-timer.h"
#include "lib/random.h"
#include "constraints.h"
#include "net/netstack.h"
#include "network_messages.h"
#include "dev/serial-line.h"
#include "dev/cc26xx-uart.h"
#include "ti-lib.h"

static uip_ipaddr_t ipaddr;
static uint8_t state;


static struct simple_udp_connection unicast_connection;

static struct trickle_timer tt;
static struct uip_udp_conn *trickle_conn;
static uip_ipaddr_t t_ipaddr;     /* destination: link-local all-nodes multicast */
static struct etimer trickle_et;

static struct network_message_t* incoming;
static struct network_message_t t_msg;

/*---------------------------------------------------------------------------*/
PROCESS(uart_reader_process, "Uart reader process");
PROCESS(trickle_sender_process, "Trickle Protocol process");
PROCESS(sink_receiver_process, "Vela sink receiver process");
/*---------------------------------------------------------------------------*/

void update_isr_priority_() {
    /* disable the UART */
    ti_lib_uart_disable(UART0_BASE);

    /* Disable all UART interrupts and clear all flags */
    /* Acknowledge UART interrupts */
    ti_lib_int_disable(INT_UART0_COMB);

     /* Clear all UART interrupts */
    ti_lib_uart_int_clear(UART0_BASE, UART_INT_OE | UART_INT_BE | UART_INT_PE | UART_INT_FE | UART_INT_RT | UART_INT_TX | UART_INT_RX | UART_INT_CTS);

    //change interrupt priority
    IntPrioritySet(INT_UART0_COMB, INT_PRI_LEVEL3);

    /* Acknowledge UART interrupts */
    ti_lib_int_enable(INT_UART0_COMB);

    /* enable the UART */
    ti_lib_uart_enable(UART0_BASE);
}

void sink_receiver_init() {
    update_isr_priority_();
    process_start(&sink_receiver_process, "Vela sink receiver process");
    process_start(&trickle_sender_process, "trickle protocol process");
    process_start(&uart_reader_process, "Uart reader process");
}
/*---------------------------------------------------------------------------*/
static void
receiver(struct simple_udp_connection *c,
         const uip_ipaddr_t *sender_addr,  // 4 byte address
         uint16_t sender_port,
         const uip_ipaddr_t *receiver_addr,
         uint16_t receiver_port,
         const uint8_t *data,
         uint16_t datalen) {
    leds_toggle(LEDS_GREEN);

    int i;
    for (i=0;i<4;i++){
        cc26xx_uart_write_byte(42);
    }
    cc26xx_uart_write_byte(sender_addr->u8[15]);
    for (i=0;i<datalen;i++){
        cc26xx_uart_write_byte(data[i]);
    }
}
/*---------------------------------------------------------------------------*/

static uip_ipaddr_t *
set_global_address(void)
{
    int i;
    uip_ip6addr(&ipaddr, UIP_DS6_DEFAULT_PREFIX, 0, 0, 0, 0, 0, 0, 0);
    uip_ds6_set_addr_iid(&ipaddr, &uip_lladdr);
    uip_ds6_addr_add(&ipaddr, 0, ADDR_AUTOCONF);

    PRINTF("IPv6 addresses: ");
    for(i = 0; i < UIP_DS6_ADDR_NB; i++) {
        state = uip_ds6_if.addr_list[i].state;
        if(uip_ds6_if.addr_list[i].isused &&
                (state == ADDR_TENTATIVE || state == ADDR_PREFERRED)) {
            uip_debug_ipaddr_print(&uip_ds6_if.addr_list[i].ipaddr);
            PRINTF("\n");
        }
    }
    return &ipaddr;
}
/*---------------------------------------------------------------------------*/
static struct uip_ds6_addr *root_if;
static rpl_dag_t *dag;
static uip_ipaddr_t prefix;
static void
create_rpl_dag(uip_ipaddr_t *ipaddr)
{
    root_if = uip_ds6_addr_lookup(ipaddr);
    if(root_if != NULL) {
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
    return 0;
}
/*---------------------------------------------------------------------------*/

static void
trickle_tx(void *ptr, uint8_t suppress)
{
     /* Instead of changing ->ripaddr around by ourselves, we could have used
     * uip_udp_packet_sendto which would have done it for us. However it puts an
     * extra ~20 bytes on stack and the cc2x3x micros hate it, so we stick with
     * send() */

    /* Destination IP: link-local all-nodes multicast */
    uip_ipaddr_copy(&trickle_conn->ripaddr, &t_ipaddr);
    uip_udp_packet_send(trickle_conn, &t_msg, sizeof(t_msg));
    /* Restore to 'accept incoming from any IP' */
    uip_create_unspecified(&trickle_conn->ripaddr);
}

static void
tcpip_handler(void)
{
    if(uip_newdata()) {
        incoming = (struct network_message_t*) uip_appdata;
        if(t_msg.pktnum == incoming->pktnum) {
            trickle_timer_consistency(&tt);
        }
        else {
            if(t_msg.pktnum < incoming->pktnum) {
                //Make sure the sink always has the highest pktnum of all tricklenodes
                t_msg.pktnum = incoming->pktnum + 1;
            }
            trickle_timer_inconsistency(&tt);
        }
    }
}

static uip_ipaddr_t * ipaddr_global;
static struct etimer uping;

PROCESS_THREAD(sink_receiver_process, ev, data)
{
    PROCESS_BEGIN();

    cc26xx_uart_set_input(dummy_uart_receiver);

    servreg_hack_init();

    ipaddr_global = set_global_address();

    create_rpl_dag(ipaddr_global);

    servreg_hack_register(SERVICE_ID, ipaddr_global);

    simple_udp_register(&unicast_connection, UDP_PORT, NULL, UDP_PORT, receiver);

    NETSTACK_RDC.off(1);

    while(1) {
    	//do nothing
        etimer_set(&uping, CLOCK_SECOND * 10);
        PROCESS_WAIT_UNTIL(etimer_expired(&uping));
    }
    PROCESS_END();
}
/*---------------------------------------------------------------------------*/
//
PROCESS_THREAD(trickle_sender_process, ev, data)
{
    PROCESS_BEGIN();
    PRINTF("Trickle protocol started\n");
    uip_create_linklocal_allnodes_mcast(&t_ipaddr); /* Store for later */
    trickle_conn = udp_new(NULL, UIP_HTONS(TRICKLE_PROTO_PORT), NULL);
    udp_bind(trickle_conn, UIP_HTONS(TRICKLE_PROTO_PORT));
    PRINTF("Connection: local/remote port %u/%u\n",
           UIP_HTONS(trickle_conn->lport), UIP_HTONS(trickle_conn->rport));

    trickle_timer_config(&tt, IMIN, IMAX, REDUNDANCY_CONST);
    trickle_timer_set(&tt, trickle_tx, &tt);

    t_msg.pktnum = 0;

    /* At this point trickle is started and is running the first interval.*/
    etimer_set(&trickle_et, CLOCK_SECOND * 15);

    while(1) {
        PROCESS_YIELD();
        if(ev == tcpip_event) {
            tcpip_handler();
        } else if(etimer_expired(&trickle_et)) {
            trickle_timer_reset_event(&tt);
            etimer_restart(&trickle_et);
        }
    }
    PROCESS_END();
}

static uint8_t size;
static uint8_t* input;

PROCESS_THREAD(uart_reader_process, ev, data)
{
    PROCESS_BEGIN();
    cc26xx_uart_init();
    serial_line_init();
    cc26xx_uart_set_input(serial_line_input_byte);

    while(1) {
        PROCESS_YIELD();
        if(ev == serial_line_event_message){
            size = ((uint8_t*)data)[0];
            input = (uint8_t*)data;

            t_msg.pkttype = 0;
            t_msg.pkttype = ((uint16_t)input[1])<<8;
            t_msg.pkttype |= input[2];
            t_msg.pktnum++;
            if(t_msg.pktnum == 254) {
                t_msg.pktnum = 0;
            }
            t_msg.payload.data_len = size-HEADER_SIZE;

            memcpy(t_msg.payload.p_data, &input[HEADER_SIZE+1], size - HEADER_SIZE);

            trickle_timer_reset_event(&tt);
            leds_toggle(LEDS_RED);
        }
    }
    PROCESS_END();
}
