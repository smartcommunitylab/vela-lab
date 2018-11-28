/**
 * vela_sender.c - this file drives the sending of RPL packets. The intent is
 * for it to be used from the UART by raising an event event_data_ready, then
 * the data will be sent
 * 
 **/
#include "contiki.h"
#include "sys/etimer.h"
#include "sys/ctimer.h"
#include "net/ip/uip.h"
#include "net/ipv6/uip-ds6.h"
#include "net/ip/uip-debug.h"
#include "net/rpl/rpl.h"
#include "sys/node-id.h"
#include "simple-udp.h"
#include "servreg-hack.h"
#include "vela_uart.h"
#include "vela_sender.h"
#include "lib/trickle-timer.h"
#include "constraints.h"
#include "dev/leds.h"
#include <stdio.h>
#include "network_messages.h"
#include "lib/random.h"

static struct etimer periodic_timer;

static struct simple_udp_connection unicast_connection;
static uint8_t message_number = 1;
static uint8_t buf[MAX_REPORT_DATA_SIZE + 2];
static uip_ipaddr_t *addr;

static bool debug = false;

static struct trickle_timer tt;
static struct uip_udp_conn *trickle_conn;
static uip_ipaddr_t t_ipaddr;     /* destination: link-local all-nodes multicast */
static struct etimer trickle_et;
struct network_message_t trickle_msg;
PROCESS(vela_sender_process, "vela sender process");
PROCESS(trickle_protocol_process, "Trickle Protocol process");

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
    printf("Data received on port %d from port %d with length %d at %lu\n",
           receiver_port, sender_port, datalen, clock_time());
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

    if (debug) printf("IPv6 addresses: ");
    for(i = 0; i < UIP_DS6_ADDR_NB; i++) {
        state = uip_ds6_if.addr_list[i].state;
        if(uip_ds6_if.addr_list[i].isused &&
                (state == ADDR_TENTATIVE || state == ADDR_PREFERRED)) {
            uip_debug_ipaddr_print(&uip_ds6_if.addr_list[i].ipaddr);
            if (debug) printf("\n");
        }
    }
}
/*---------------------------------------------------------------------------*/
void vela_sender_init() {
    random_init(0);

    printf("vela_sender: initializing \n");
    message_number = 0;

    servreg_hack_init();

    set_global_address();

    simple_udp_register(&unicast_connection, UDP_PORT, NULL, UDP_PORT, receiver);

    process_start(&vela_sender_process, "vela sender process");
    process_start(&trickle_protocol_process, "trickle protocol process");
    return;
}

/*---------------------------------------------------------------------------*/
static uint8_t send_to_sink(struct network_message_t n_msg) {
    addr = servreg_hack_lookup(SERVICE_ID);
    leds_toggle(LEDS_GREEN);
    if(addr!=NULL){
        printf("sendingsink...\n");
        if (++message_number == 254)  message_number = 1; //message_number is between 1 and 253
        n_msg.pktnum = message_number;
        buf[0] = n_msg.pkttype >> 8;
        buf[1] = n_msg.pkttype;
        buf[2] = message_number;
        memcpy(&buf[3], n_msg.payload.p_data, n_msg.payload.data_len);
        printf("Sending size: %u, msgNum: %u ,first 2 byte: 0x%02x 0x%02x\n",n_msg.payload.data_len,message_number, buf[0], buf[1]);
        simple_udp_sendto(&unicast_connection, buf, n_msg.payload.data_len+3, addr);
        leds_on(LEDS_RED);
        return 0;
    } else {
        leds_off(LEDS_RED);
        printf("ERROR: No sink available!\n");
        return -1;
    }
}

static void
trickle_tx(void *ptr, uint8_t suppress)
{
    if(suppress == TRICKLE_TIMER_TX_SUPPRESS) {
        return;
    }
    uip_ipaddr_copy(&trickle_conn->ripaddr, &t_ipaddr);
    uip_create_unspecified(&trickle_conn->ripaddr);
}

static void
tcpip_handler(void)
{
    if(uip_newdata()) {
        struct network_message_t *incoming = (struct network_message_t *) uip_appdata;
        if(trickle_msg.pktnum == incoming->pktnum) {
            trickle_timer_consistency(&tt);
        }
        else {
            if(incoming->pktnum > trickle_msg.pktnum){
                trickle_msg = *incoming;
                switch(trickle_msg.pkttype) {
                case network_request_ping: {
                    ;
                    printf("Ping request\n");
                    process_post(&cc2650_uart_process, event_ping_requested, &incoming->payload.p_data[0]);
                    break;
                }
                default:
                    break;
                }
            }
            trickle_timer_inconsistency(&tt);
        }
    }
    return;
}

PROCESS_THREAD(vela_sender_process, ev, data) { 
    static struct network_message_t response;
    static struct etimer pause_timer;
    static data_t* eventData;
    static int offset = 0;
    static int sizeToSend = 0;
    PROCESS_BEGIN();
    etimer_set(&pause_timer, TIME_BETWEEN_SENDS);

    event_buffer_empty = process_alloc_event();
    event_bat_data_ready = process_alloc_event();

    if (debug) printf("unicast: started\n");
    while (1)
    {
        PROCESS_WAIT_EVENT();
        if(ev == event_data_ready) {
            eventData = (data_t*)data;
            offset=0; // offset from the begging of the buffer to send next
            sizeToSend = (int)eventData->data_len; // amount of data in the buffer not yet sent
            struct network_message_t chunk;
            while ( sizeToSend > 0 ) {
                if (sizeToSend > MAX_REPORT_DATA_SIZE) {
                    if(offset==0) {
                        memcpy(buf, eventData->p_data, MAX_REPORT_DATA_SIZE + 2);
                        chunk.pkttype = network_new_sequence;
                        chunk.payload.data_len = MAX_REPORT_DATA_SIZE + 2;
                        memcpy(chunk.payload.p_data, buf, chunk.payload.data_len);
                        uint8_t ret = send_to_sink(chunk);
                        if(ret==0){
                            sizeToSend -= MAX_REPORT_DATA_SIZE + 2;
                            offset += MAX_REPORT_DATA_SIZE + 2;
                        }
                    }
                    else {
                        memcpy(&buf, &eventData->p_data[offset], MAX_REPORT_DATA_SIZE);
                        chunk.pkttype = network_active_sequence;
                        chunk.payload.data_len = MAX_REPORT_DATA_SIZE;
                        memcpy(chunk.payload.p_data, buf, chunk.payload.data_len);
                        uint8_t ret = send_to_sink(chunk);
                        if(ret==0){
                            sizeToSend -= MAX_REPORT_DATA_SIZE;
                            offset += MAX_REPORT_DATA_SIZE;
                        }
                    }
                    etimer_set(&periodic_timer, TIME_BETWEEN_SENDS);
                    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));
                }
                else {
                    // this is my last packet
                    memcpy(&buf, &eventData->p_data[offset], sizeToSend);
                    chunk.pkttype = network_last_sequence;
                    chunk.payload.data_len = sizeToSend;
                    memcpy(chunk.payload.p_data, buf, sizeToSend);
                    send_to_sink(chunk);
                    sizeToSend = 0;
                }
            }
        }
        if(ev == event_pong_received){
            printf("Sending pong\n");
            uint8_t* temp = (uint8_t*)data;
            response.pkttype = network_respond_ping;
            response.payload.data_len = 1;
            response.payload.p_data[0] = temp[0];
            send_to_sink(response);
        }
        if(ev == event_bat_data_ready) {
            printf("Received bat data\n");
            struct network_message_t message;
            message.pkttype = network_bat_data;
            memcpy(message.payload.p_data, data, 10);
            message.payload.data_len = 10;
            send_to_sink(message);
        }
    }
    PROCESS_END();
}

PROCESS_THREAD(trickle_protocol_process, ev, data)
{
    PROCESS_BEGIN();
    printf("Trickle protocol started\n");
    //initialize trickle_msg struct
    trickle_msg.pkttype = 0;
    trickle_msg.pktnum = 0;
    uint8_t temp[1] = {0};
    trickle_msg.payload.p_data[0] = temp[0];
    trickle_msg.payload.data_len = 1;

    uip_create_linklocal_allnodes_mcast(&t_ipaddr); /* Store for later */
    trickle_conn = udp_new(NULL, UIP_HTONS(TRICKLE_PROTO_PORT), NULL);
    udp_bind(trickle_conn, UIP_HTONS(TRICKLE_PROTO_PORT));
    printf("Connection: local/remote port %u/%u\n",
           UIP_HTONS(trickle_conn->lport), UIP_HTONS(trickle_conn->rport));

    trickle_timer_config(&tt, IMIN, IMAX, REDUNDANCY_CONST);
    trickle_timer_set(&tt, trickle_tx, &tt);

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
