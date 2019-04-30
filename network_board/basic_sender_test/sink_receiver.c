/*
 * Copyright (c) 2011, Swedish Institute of Computer Science.
 * All rights reserved.
 * This file is part of the Contiki operating system.
 */
#include <stdio.h>
#include <string.h>
#include "contiki.h"
#include "contiki-net.h"
#include "contiki-lib.h"
#include "dev/leds.h"
#include "dev/cc26xx-uart.h"
#include "dev/serial-line.h"
#include "lib/trickle-timer.h"
#include "constraints.h"
#include "network_messages.h"
#include "ti-lib.h"
//#include "servreg-hack.h"

#include "sys/log.h"
#define LOG_MODULE "sink_receiver"
#define LOG_LEVEL LOG_LEVEL_DBG

#define TO_BYTE(hex_data) (((hex_data ) <= '9') ? ((hex_data)  - '0') : ((hex_data)  - 'A' + 10))
#define TO_HEX(byte_data) (((byte_data) <=  9 ) ? ((byte_data) + '0') : ((byte_data) + 'A' - 10)) //NOTE THAT THIS PROPERLY WORKS ONLY ON 4 BITS!!!
#define UART_PKT_START_SYMBOL           '\x02'      //start of text - this is the same as in uart_util.h

static uip_ipaddr_t ipaddr;
static uint8_t state;


static struct simple_udp_connection unicast_connection;

static struct trickle_timer tt;
static struct uip_udp_conn *trickle_conn;
static uip_ipaddr_t t_ipaddr;     /* destination: link-local all-nodes multicast */
static struct etimer trickle_et;

static network_message_t* incoming;
static network_message_t trickle_msg;

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
static void sendCharAsHexString(uint8_t c){
    uint8_t hc = TO_HEX((c>>4)&0x0F);
    cc26xx_uart_write_byte( hc );
    hc = TO_HEX(c & 0x0F) ;
    cc26xx_uart_write_byte( hc );
}

static void
receiver(struct simple_udp_connection *c,
         const uip_ipaddr_t *sender_addr,  // 4 byte address
         uint16_t sender_port,
         const uip_ipaddr_t *receiver_addr,
         uint16_t receiver_port,
         const uint8_t *data,
         uint16_t datalen) {
    leds_toggle(LEDS_GREEN);

    cc26xx_uart_write_byte(UART_PKT_START_SYMBOL);
    sendCharAsHexString(sender_addr->u8[15]);
    for (uint16_t i=0;i<datalen;i++){
        sendCharAsHexString(data[i]);
    }
    cc26xx_uart_write_byte('\n');
}
/*---------------------------------------------------------------------------*/

static uip_ipaddr_t *
set_global_address(void)
{
    int i;
    uip_ip6addr(&ipaddr, UIP_DS6_DEFAULT_PREFIX, 0, 0, 0, 0, 0, 0, 0);
    uip_ds6_set_addr_iid(&ipaddr, &uip_lladdr);
    uip_ds6_addr_add(&ipaddr, 0, ADDR_AUTOCONF);

    LOG_INFO("IPv6 addresses: ");
    for(i = 0; i < UIP_DS6_ADDR_NB; i++) {
        state = uip_ds6_if.addr_list[i].state;
        if(uip_ds6_if.addr_list[i].isused &&
                (state == ADDR_TENTATIVE || state == ADDR_PREFERRED)) {
            LOG_INFO_6ADDR(&uip_ds6_if.addr_list[i].ipaddr);
            LOG_INFO_("\n");
        }
    }
    return &ipaddr;
}
/*---------------------------------------------------------------------------*/
//static struct uip_ds6_addr *root_if;
//static rpl_dag_t *dag;
//static uip_ipaddr_t prefix;
//static void
//create_rpl_dag(uip_ipaddr_t *ipaddr)
//{
//    root_if = uip_ds6_addr_lookup(ipaddr);
//    if(root_if != NULL) {
//        rpl_set_root(RPL_DEFAULT_INSTANCE, ipaddr);
//        dag = rpl_get_any_dag();
//        uip_ip6addr(&prefix, UIP_DS6_DEFAULT_PREFIX, 0, 0, 0, 0, 0, 0, 0);
//        rpl_set_prefix(dag, &prefix, 64);
//        PRINTF("created a new RPL dag\n");
//    } else {
//        PRINTF("failed to create a new RPL DAG\n");
//    }
//}
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
    uip_udp_packet_send(trickle_conn, &trickle_msg, sizeof(trickle_msg)-sizeof(trickle_msg.payload.p_data)+trickle_msg.payload.data_len);
    /* Restore to 'accept incoming from any IP' */
    uip_create_unspecified(&trickle_conn->ripaddr);
}

static void
tcpip_handler(void)
{
    if(uip_newdata()) {
        incoming = (network_message_t*) uip_appdata;
        if(trickle_msg.pktnum == incoming->pktnum) {
            trickle_timer_consistency(&tt);
        }
        else {
            if(trickle_msg.pktnum < incoming->pktnum) {
                //Make sure the sink always has the highest pktnum of all tricklenodes
                trickle_msg.pktnum = incoming->pktnum + 1;
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

//    servreg_hack_init();

    ipaddr_global = set_global_address();

    //create_rpl_dag(ipaddr_global);
    NETSTACK_ROUTING.root_start(); //TODO: is this the same of create_rpl_dag?

//    servreg_hack_register(SERVICE_ID, ipaddr_global);

    simple_udp_register(&unicast_connection, UDP_PORT, NULL, UDP_PORT, receiver);

    //NETSTACK_MAC.off();

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
    LOG_INFO("Trickle protocol started\n");
    uip_create_linklocal_allnodes_mcast(&t_ipaddr); /* Store for later */
    trickle_conn = udp_new(NULL, UIP_HTONS(TRICKLE_PROTO_PORT), NULL);
    udp_bind(trickle_conn, UIP_HTONS(TRICKLE_PROTO_PORT));
    LOG_INFO("Connection: local/remote port %u/%u\n",
           UIP_HTONS(trickle_conn->lport), UIP_HTONS(trickle_conn->rport));

    trickle_timer_config(&tt, TRICKLE_IMIN, TRICKLE_IMAX, REDUNDANCY_CONST);
    trickle_timer_set(&tt, trickle_tx, &tt);

    trickle_msg.pktnum = 0;

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

static uint8_t payload_size;
static uint16_t serial_line_len;
static uint8_t* input;
static uint8_t byte_array[SERIAL_LINE_CONF_BUFSIZE/2];

PROCESS_THREAD(uart_reader_process, ev, data)
{
    PROCESS_BEGIN();
    cc26xx_uart_init();
    serial_line_init();
    cc26xx_uart_set_input(serial_line_input_byte);

    while(1) {
        PROCESS_YIELD();
        if(ev == serial_line_event_message){

            input=(uint8_t*)data;
            serial_line_len = strlen((char*)input);

            if(serial_line_len>2 && serial_line_len<SERIAL_LINE_CONF_BUFSIZE/2){
                for(uint8_t ci=0;ci<serial_line_len/2;ci++){
                    byte_array[ci] = ((0x0F & TO_BYTE(input[2*ci])) << 4) + ( 0x0F & TO_BYTE(input[2*ci + 1]));
                }

                payload_size=byte_array[0];

                trickle_msg.pkttype = ((uint16_t) byte_array[1]) << 8;
                trickle_msg.pkttype |= byte_array[2];
                trickle_msg.pktnum++;
                trickle_msg.payload.data_len=payload_size;
                memcpy(trickle_msg.payload.p_data, &byte_array[NET_MESS_MSGTYPE_LEN+NET_MESS_MSGLEN_LEN], payload_size);

                trickle_timer_reset_event(&tt);
                leds_toggle(LEDS_RED);
            }
        }
    }
    PROCESS_END();
}
