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
#ifdef BOARD_LAUNCHPAD_VELA
#if BOARD_LAUNCHPAD_VELA==1
#include "max-17260-sensor.h"
#endif
#endif


static struct etimer periodic_timer;

static struct simple_udp_connection unicast_connection;
static uint8_t message_number = 1;
static uint8_t buf[MAX_REPORT_DATA_SIZE + 10];
static uip_ipaddr_t *addr;

static bool debug = false;

static struct trickle_timer tt;
static struct uip_udp_conn *trickle_conn;
static uip_ipaddr_t t_ipaddr;     /* destination: link-local all-nodes multicast */
static struct etimer trickle_et;
struct network_message_t trickle_msg;

static struct etimer keep_alive_timer;
static struct etimer delay_ping_timer;

static uint8_t keep_alive_interval = KEEP_ALIVE_INTERVAL;

PROCESS(vela_sender_process, "vela sender process");
PROCESS(trickle_protocol_process, "Trickle Protocol process");
PROCESS(keep_alive_process, "keep alive process");

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
static uip_ipaddr_t ipaddr;
static uint8_t state;

static void
set_global_address(void)
{

    int i;
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
    process_start(&keep_alive_process, "keep alive process");
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

        printf("Sending size: %u, msgNum: %u ,first 2 byte: 0x%02x 0x%02x to ",n_msg.payload.data_len,message_number, buf[3], buf[4]);
        static rpl_dag_t* dag;
        dag = rpl_get_any_dag();
        if(dag->preferred_parent != NULL) {
            printf("Preferred parent: ");
            uip_debug_ipaddr_print(rpl_get_parent_ipaddr(dag->preferred_parent));
            printf("\n");
        }
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
    printf("Trickle TX\n");
    uip_ipaddr_copy(&trickle_conn->ripaddr, &t_ipaddr);
    uip_udp_packet_send(trickle_conn, &trickle_msg, sizeof(trickle_msg));
    uip_create_unspecified(&trickle_conn->ripaddr);
}

static struct network_message_t *incoming;
static rpl_dag_t* dag;
static void
tcpip_handler(void)
{
    if(uip_newdata()) {
    	dag = rpl_get_any_dag();
    	if(dag != NULL && dag->preferred_parent != NULL) {
    	    printf("Preferred parent: ");
    	    uip_debug_ipaddr_print(rpl_get_parent_ipaddr(dag->preferred_parent));
    	    printf("\n");
    	}
        incoming = (struct network_message_t *) uip_appdata;
    	printf("Received new Trickle message, counter: %d, type: %X, payload: %d\n", incoming->pktnum, incoming->pkttype, incoming->payload.p_data[0]);

        if(trickle_msg.pktnum == incoming->pktnum) {
            trickle_timer_consistency(&tt);
        }
        else {
            trickle_timer_inconsistency(&tt);
            printf("At %lu: Trickle inconsistency. Scheduled TX for %lu\n",
                         (unsigned long)clock_time(),
                         (unsigned long)(tt.ct.etimer.timer.start +
            tt.ct.etimer.timer.interval));
            if(incoming->pktnum > trickle_msg.pktnum || (trickle_msg.pktnum - incoming->pktnum > 10)){
                trickle_msg = *incoming;
            	printf("Received new message, type: %X\n", trickle_msg.pkttype);
            	switch(trickle_msg.pkttype) {
                case network_request_ping: {
                    ;
                    printf("Ping request\n");
                    process_post(&cc2650_uart_process, event_ping_requested, &incoming->payload.p_data[0]);
//                    struct network_message_t response;
//                    response.pkttype = network_respond_ping;
//					response.payload.data_len = 1;
//					response.payload.p_data[0] = 233;
//					send_to_sink(response);
                    break;
                }
                case nordic_turn_bt_off: {
                	;
                	printf("Turning Bluetooth off...\n");
            		process_post(&cc2650_uart_process, turn_bt_off, NULL);
            		break;
				}
				case nordic_turn_bt_on: {
					;
					printf("Turning Bluetooth on...\n");
					process_post(&cc2650_uart_process, turn_bt_on, NULL);
					break;
				}
				case nordic_turn_bt_on_low: {
					;
					printf("Turning Bluetooth on...\n");
					process_post(&cc2650_uart_process, turn_bt_on_low, NULL);
					break;
				}
				case nordic_turn_bt_on_def: {
					;
					printf("Turning Bluetooth on...\n");
					process_post(&cc2650_uart_process, turn_bt_on_def, NULL);
					break;
				}
				case nordic_turn_bt_on_high: {
					;
					printf("Turning Bluetooth on...\n");
					process_post(&cc2650_uart_process, turn_bt_on_high, NULL);
					break;
				}
				case ti_set_keep_alive: {
					;
					printf("Setting keep alive interval to %hhu\n", incoming->payload.p_data[0]);
					keep_alive_interval = (uint8_t)incoming->payload.p_data[0];
					etimer_set(&keep_alive_timer, keep_alive_interval * CLOCK_SECOND);
					break;
				}
				default:
                    break;
                }
            }
        }
    }
    return;
}

static struct network_message_t response;
static struct etimer pause_timer;
static data_t* eventData;
static int offset = 0;
static int sizeToSend = 0;
static struct network_message_t chunk;
static uint8_t ret;
static uint8_t* temp;
static struct network_message_t bat_message;

PROCESS_THREAD(vela_sender_process, ev, data) {
    PROCESS_BEGIN();
    etimer_set(&pause_timer, TIME_BETWEEN_SENDS);

    event_buffer_empty = process_alloc_event();
    //event_bat_data_ready = process_alloc_event();

    if (debug) printf("unicast: started\n");
    while (1)
    {
        PROCESS_WAIT_EVENT();

        if(ev == event_data_ready) {
            eventData = (data_t*)data;
            offset=0; // offset from the begining of the buffer to send next
            sizeToSend = (int)eventData->data_len; // amount of data in the buffer not yet sent
            while ( sizeToSend > 0 ) {
                if (sizeToSend > MAX_REPORT_DATA_SIZE) {
                    if(offset==0) {
                    	  chunk.payload.p_data[0] = (uint8_t)(sizeToSend >> 8);
                    	  chunk.payload.p_data[1] = (uint8_t)sizeToSend;
                    	  chunk.pkttype = network_new_sequence;
                    	  chunk.payload.data_len = MAX_REPORT_DATA_SIZE + 2;
                    	  memcpy(&chunk.payload.p_data[2], &eventData->p_data[0], MAX_REPORT_DATA_SIZE);
                    	  ret = send_to_sink(chunk);
                    	  if(ret==0){
                    		  sizeToSend -= MAX_REPORT_DATA_SIZE;
                    	      offset += MAX_REPORT_DATA_SIZE;
                    	  }
                    }
                    else {
                        chunk.pkttype = network_active_sequence;
                        chunk.payload.data_len = MAX_REPORT_DATA_SIZE;
                        memcpy(chunk.payload.p_data, &eventData->p_data[offset], chunk.payload.data_len);
                        ret = send_to_sink(chunk);
                        if(ret==0){
                            sizeToSend -= MAX_REPORT_DATA_SIZE;
                            offset += MAX_REPORT_DATA_SIZE;
                        }
                    }
                    etimer_set(&periodic_timer, TIME_BETWEEN_SENDS);
                    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));
                }
                else {
					if (offset == 0) {
						chunk.payload.p_data[0] = (uint8_t) (sizeToSend >> 8);
						chunk.payload.p_data[1] = (uint8_t) sizeToSend;
						chunk.pkttype = network_new_sequence;
						chunk.payload.data_len = sizeToSend + 2;
						memcpy(&chunk.payload.p_data[2], &eventData->p_data[0], sizeToSend);
						ret = send_to_sink(chunk);
						if (ret == 0) {
							sizeToSend = 0;
						}
					}
					else {
						// this is my last packet
						chunk.pkttype = network_last_sequence;
						chunk.payload.data_len = sizeToSend;
						memcpy(chunk.payload.p_data, &eventData->p_data[offset], sizeToSend);
						send_to_sink(chunk);
						sizeToSend = 0;
					}
                }
            }
        }
        if(ev == event_pong_received){
            printf("Sending pong\n");
            temp = (uint8_t*)data;
            response.pkttype = network_respond_ping;
            response.payload.data_len = 1;
            response.payload.p_data[0] = temp[0];
            etimer_set(&delay_ping_timer, random_rand() % (2 * CLOCK_SECOND));
            PROCESS_WAIT_UNTIL(etimer_expired(&delay_ping_timer));
            send_to_sink(response);
        }
        if(ev == event_bat_data_ready) {
            printf("Received bat data\n");
            bat_message.pkttype = network_bat_data;
            memcpy(bat_message.payload.p_data, data, 12);
            bat_message.payload.data_len = 12;
            send_to_sink(bat_message);
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

#ifdef BOARD_LAUNCHPAD_VELA
#if BOARD_LAUNCHPAD_VELA==1
    	static uint16_t REP_CAP_mAh;
    	static uint16_t REP_SOC_permillis;
#endif
#else
    	static uint16_t bat_data1 = 0;
    	static uint16_t bat_data2 = 1;
#endif
static struct network_message_t keep_alive_msg;

PROCESS_THREAD(keep_alive_process, ev, data)
{
    PROCESS_BEGIN();
    etimer_set(&keep_alive_timer, keep_alive_interval * CLOCK_SECOND);

    keep_alive_msg.pkttype = network_keep_alive;
    keep_alive_msg.payload.data_len = 4;
    while(1) {
    	PROCESS_WAIT_UNTIL(etimer_expired(&keep_alive_timer));
    	etimer_set(&keep_alive_timer, keep_alive_interval * CLOCK_SECOND);
#ifdef BOARD_LAUNCHPAD_VELA
#if BOARD_LAUNCHPAD_VELA==1
    	REP_CAP_mAh = max_17260_sensor.value(MAX_17260_SENSOR_TYPE_REP_CAP);
    	REP_SOC_permillis = max_17260_sensor.value(MAX_17260_SENSOR_TYPE_REP_SOC);
    	keep_alive_msg.payload.p_data[0] = (uint8_t)REP_CAP_mAh >> 8;
    	keep_alive_msg.payload.p_data[1] = (uint8_t)REP_CAP_mAh;
    	keep_alive_msg.payload.p_data[2] = (uint8_t)REP_SOC_permillis >> 8;
    	keep_alive_msg.payload.p_data[3] = (uint8_t)REP_SOC_permillis;
#endif
#else
    	bat_data1++;
    	bat_data2++;
    	keep_alive_msg.payload.p_data[0] = (uint8_t)bat_data1 >> 8;
    	keep_alive_msg.payload.p_data[1] = (uint8_t)bat_data1;
    	keep_alive_msg.payload.p_data[2] = (uint8_t)bat_data2 >> 8;
    	keep_alive_msg.payload.p_data[3] = (uint8_t)bat_data2;
#endif
    	send_to_sink(keep_alive_msg);
    }
    PROCESS_END();
}
