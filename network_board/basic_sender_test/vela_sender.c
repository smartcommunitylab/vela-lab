/**
 * vela_sender.c - this file drives the sending of RPL packets. The intent is
 * for it to be used from the UART by raising an event event_data_ready, then
 * the data will be sent
 * 
 **/

#include <stdio.h>
#include <string.h>
#include "contiki.h"
#include "contiki-net.h"
#include "contiki-lib.h"
#include "sys/node-id.h"
#include "dev/leds.h"
#include "lib/trickle-timer.h"
#include "network_messages.h"
//#include "servreg-hack.h"
#include "vela_node.h"
#include "vela_uart.h"
#include "vela_sender.h"

#include "sys/log.h"
#define LOG_MODULE "vela_sender"
#define LOG_LEVEL LOG_LEVEL_DBG

#ifdef BOARD_LAUNCHPAD_VELA
#if BOARD_LAUNCHPAD_VELA==1
#include "max-17260-sensor.h"
#endif
#endif

#define VERY_LONG_TIMER_VALUE   14510024
#define MAX_RANDOM_DELAY_S      0.5f

static struct etimer periodic_timer;

static struct simple_udp_connection unicast_connection;
static uint8_t message_number = 1;
static uint8_t buf[MAX_PACKET_SIZE + 10];
static uip_ipaddr_t addr;


static struct trickle_timer tt;
static struct uip_udp_conn *trickle_conn;
static uip_ipaddr_t t_ipaddr;     /* destination: link-local all-nodes multicast */
static struct etimer trickle_et;
network_message_t trickle_msg;

static struct etimer keep_alive_timer;
//static struct etimer rand_delay_timer;

static uint32_t keep_alive_interval = KEEP_ALIVE_INTERVAL;

static uint32_t time_between_sends = TIME_BETWEEN_SENDS_DEFAULT_S*CLOCK_SECOND;

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
    LOG_INFO("Data received on port %d from port %d with length %d at %lu\n",
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

    LOG_INFO("IPv6 addresses: ");
    for(i = 0; i < UIP_DS6_ADDR_NB; i++) {
        state = uip_ds6_if.addr_list[i].state;
        if(uip_ds6_if.addr_list[i].isused &&
                (state == ADDR_TENTATIVE || state == ADDR_PREFERRED)) {
            LOG_INFO_6ADDR(&uip_ds6_if.addr_list[i].ipaddr);
            LOG_INFO_("\n");
        }
    }
}
/*---------------------------------------------------------------------------*/
void vela_sender_init() {
    random_init(0);

    LOG_INFO("vela_sender: initializing \n");
    message_number = 0;

//    servreg_hack_init();

    set_global_address();

    simple_udp_register(&unicast_connection, UDP_PORT, NULL, UDP_PORT, receiver);

    process_start(&vela_sender_process, "vela sender process");
    process_start(&trickle_protocol_process, "trickle protocol process");
    process_start(&keep_alive_process, "keep alive process");
    return;
}

/*---------------------------------------------------------------------------*/
static uint8_t send_to_sink(network_message_t n_msg) {
//    addr = servreg_hack_lookup(SERVICE_ID);
    //NETSTACK_ROUTING.get_root_ipaddr(addr);
    leds_toggle(LEDS_GREEN);
    if(NETSTACK_ROUTING.node_is_reachable() && NETSTACK_ROUTING.get_root_ipaddr(&addr)){
        LOG_DBG("sendingsink...\n");
        message_number++;
        buf[0] = n_msg.pkttype >> 8;
        buf[1] = n_msg.pkttype;
        buf[2] = message_number;
        memcpy(&buf[3], n_msg.payload.p_data, n_msg.payload.data_len);

        LOG_DBG("Sending size: %u, msgNum: %u ,first 2 byte: 0x%02x 0x%02x to ",n_msg.payload.data_len,message_number, buf[3], buf[4]);
        LOG_DBG_6ADDR(&addr);
        LOG_DBG_("\n");

        simple_udp_sendto(&unicast_connection, buf, n_msg.payload.data_len+3, &addr);
        leds_on(LEDS_RED);
        return 0;
    } else {
        leds_off(LEDS_RED);
        LOG_WARN("ERROR: No sink available!\n");
        return -1;
    }
}

static void
trickle_tx(void *ptr, uint8_t suppress)
{
    LOG_DBG("Trickle TX\n");
    uip_ipaddr_copy(&trickle_conn->ripaddr, &t_ipaddr);
    uip_udp_packet_send(trickle_conn, &trickle_msg, sizeof(trickle_msg)-sizeof(trickle_msg.payload.p_data)+trickle_msg.payload.data_len);
    uip_create_unspecified(&trickle_conn->ripaddr);
}

static void node_delay(void){
//    uint32_t delayTime=(MAX_RANDOM_DELAY_S*CLOCK_SECOND*(node_id & (0x00FF)))/256;
    uint32_t delayTime=(random_rand()*MAX_RANDOM_DELAY_S*CLOCK_SECOND)/0xFFFF;
    LOG_DBG("node_id 0x%4X, PACKET Delay %lu\n",node_id, delayTime);
    clock_wait(delayTime);
    //Commented the more appropriate wait implementation. It doesn't work because of some problems with context switch
    //clock_wait() is enough since it is only unatantum
//    PROCESS_CONTEXT_BEGIN(trickle_protocol_process);
//    etimer_set(&rand_delay_timer, delayTime);
//    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&rand_delay_timer));
//    PROCESS_CONTEXT_END(trickle_protocol_process);
}

static network_message_t *incoming;
static void
tcpip_handler(void)
{
    if(uip_newdata()) {
        incoming = (network_message_t *) uip_appdata;
        LOG_INFO("Received new Trickle message, counter: %d, type: %X, payload: ", incoming->pktnum, incoming->pkttype);

    	uint8_t asd=0;
    	for(asd=0;asd<incoming->payload.data_len;asd++){
    	    LOG_INFO_("%02X", incoming->payload.p_data[asd]);
    	}
    	LOG_INFO_("\n");

        if(trickle_msg.pktnum == incoming->pktnum) {
            trickle_timer_consistency(&tt);
        }
        else {
            trickle_timer_inconsistency(&tt);
            LOG_WARN("At %lu: Trickle inconsistency. Scheduled TX for %lu\n",
                         (unsigned long)clock_time(),
                         (unsigned long)(tt.ct.etimer.timer.start +
            tt.ct.etimer.timer.interval));

            node_delay(); //add a small delay to reduce any occurance of collision

            if(incoming->pktnum > trickle_msg.pktnum || (trickle_msg.pktnum - incoming->pktnum > 10)){
                memcpy(&trickle_msg, incoming, sizeof(network_message_t));
                LOG_DBG("Received new message, type: %X\n", trickle_msg.pkttype);
            	switch(incoming->pkttype) {
                case network_request_ping: {
                    ;
                    LOG_INFO("Ping request\n");
                    process_post(&cc2650_uart_process, event_ping_requested, incoming->payload.p_data);
                    break;
                }
                case nordic_turn_bt_off: {
                	;
                	LOG_INFO("Turning Bluetooth off...\n");
            		process_post(&cc2650_uart_process, turn_bt_off, NULL);
            		break;
				}
				case nordic_turn_bt_on: {
					;
					LOG_INFO("Turning Bluetooth on...\n");
					process_post(&cc2650_uart_process, turn_bt_on, NULL);
					break;
				}
				case nordic_turn_bt_on_low: {
					;
					LOG_INFO("Turning Bluetooth on...\n");
					process_post(&cc2650_uart_process, turn_bt_on_low, NULL);
					break;
				}
				case nordic_turn_bt_on_def: {
					;
					LOG_INFO("Turning Bluetooth on...\n");
					process_post(&cc2650_uart_process, turn_bt_on_def, NULL);
					break;
				}
				case nordic_turn_bt_on_high: {
					;
					LOG_INFO("Turning Bluetooth on...\n");
					process_post(&cc2650_uart_process, turn_bt_on_high, NULL);
					break;
				}
                case nordic_turn_bt_on_w_params: {
                    ;
                    LOG_INFO("Turning Bluetooth on...\n");
                    process_post(&cc2650_uart_process, turn_bt_on_w_params, incoming->payload.p_data);
                    break;
                }
                case ti_set_keep_alive: {
                    ;
                    LOG_INFO("Setting keep alive interval to %hhu\n", incoming->payload.p_data[0]);
                    PROCESS_CONTEXT_BEGIN(&keep_alive_process);
                    keep_alive_interval = incoming->payload.p_data[0];
                    if(keep_alive_interval > 9){
                        etimer_set(&keep_alive_timer, keep_alive_interval * CLOCK_SECOND);
                    }else{
                        etimer_set(&keep_alive_timer, VERY_LONG_TIMER_VALUE * CLOCK_SECOND);
                        LOG_INFO("Not a valid interval, turning keep alive off\n");
                    }
                    PROCESS_CONTEXT_END(&keep_alive_process);
                    break;
                }
                case ti_set_batt_info_int: {
                    ;
                    if(incoming->payload.p_data[0] != 0){
                        LOG_INFO("Enabling battery info report with interval %u\n",incoming->payload.p_data[0]);
                    }else{
                        LOG_INFO("Disabling battery info report\n");
                    }
                    process_post(&vela_node_process, set_battery_info_interval, incoming->payload.p_data);
                    break;
                }
                case network_set_time_between_sends: {
                    uint16_t time_between_sends_ms = (incoming->payload.p_data[0]<<8) | incoming->payload.p_data[1];
                    time_between_sends = time_between_sends_ms*(CLOCK_SECOND/1000.0);
                    LOG_INFO("Setting Time Between Sends to %d ms, %lu ticks\n",time_between_sends_ms,time_between_sends);
                    break;
                }
                case nordic_reset: {
                    ;
                    LOG_INFO("Reseting nordic board\n");
                    process_post(&cc2650_uart_process, reset_bt, NULL);
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

static network_message_t response;
//static struct etimer pause_timer;
static data_t* eventData;
static int offset = 0;
static int sizeToSend = 0;
static network_message_t chunk;
static uint8_t ret;
static uint8_t* temp;
static network_message_t bat_message;

PROCESS_THREAD(vela_sender_process, ev, data) {
    PROCESS_BEGIN();
    //etimer_set(&pause_timer, TIME_BETWEEN_SENDS_DEFAULT);   //what's this?

    event_buffer_empty = process_alloc_event();
    //event_bat_data_ready = process_alloc_event();

    LOG_INFO("unicast: started\n");
    while (1)
    {
        PROCESS_WAIT_EVENT();

        if(ev == event_data_ready) {
            eventData = (data_t*)data;
            offset=0; // offset from the begining of the buffer to send next
            sizeToSend = (int)eventData->data_len; // amount of data in the buffer not yet sent
            while ( sizeToSend > 0 ) {
                if (sizeToSend > MAX_PACKET_SIZE) {
                    if(offset==0) {
                    	  chunk.payload.p_data[0] = (uint8_t)(sizeToSend >> 8);
                    	  chunk.payload.p_data[1] = (uint8_t)sizeToSend;
                    	  chunk.pkttype = network_new_sequence;
                    	  chunk.payload.data_len = MAX_PACKET_SIZE + 2 - SINGLE_NODE_REPORT_SIZE; //One report less in package with header file else it wont fit
                    	  memcpy(&chunk.payload.p_data[2], &eventData->p_data[0], chunk.payload.data_len - 2);
                    	  ret = send_to_sink(chunk);
                    	  if(ret==0){
                    		  sizeToSend -= MAX_PACKET_SIZE - SINGLE_NODE_REPORT_SIZE;
                    	      offset = MAX_PACKET_SIZE - SINGLE_NODE_REPORT_SIZE;
                    	  }
                    }
                    else {
                        chunk.pkttype = network_active_sequence;
                        chunk.payload.data_len = MAX_PACKET_SIZE;
                        memcpy(chunk.payload.p_data, &eventData->p_data[offset], chunk.payload.data_len);
                        ret = send_to_sink(chunk);
                        if(ret==0){
                            sizeToSend -= MAX_PACKET_SIZE;
                            offset += MAX_PACKET_SIZE;
                        }
                    }
                    etimer_set(&periodic_timer, time_between_sends);
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
						if (ret == 0) {
						    sizeToSend = 0;
						}
					}
                }
            }
        }
        if(ev == event_pong_received){
            LOG_INFO("Sending pong\n");
            temp = (uint8_t*)data;
            response.pkttype = network_respond_ping;
            response.payload.data_len = 1;
            response.payload.p_data[0] = temp[0];
            send_to_sink(response);
        }
        if(ev == event_bat_data_ready) {
            LOG_INFO("Received battery data event\n");
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
    LOG_INFO("Trickle protocol started\n");
    //initialize trickle_msg struct
    trickle_msg.pkttype = 0;
    trickle_msg.pktnum = 0;
    uint8_t temp[1] = {0};
    trickle_msg.payload.p_data[0] = temp[0];
    trickle_msg.payload.data_len = 1;

    uip_create_linklocal_allnodes_mcast(&t_ipaddr); /* Store for later */
    trickle_conn = udp_new(NULL, UIP_HTONS(TRICKLE_PROTO_PORT), NULL);
    udp_bind(trickle_conn, UIP_HTONS(TRICKLE_PROTO_PORT));
    LOG_INFO("Connection: local/remote port %u/%u\n",
           UIP_HTONS(trickle_conn->lport), UIP_HTONS(trickle_conn->rport));

    trickle_timer_config(&tt, TRICKLE_IMIN, TRICKLE_IMAX, REDUNDANCY_CONST);
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
    	static uint16_t AVG_voltage_mV;
#endif
#else
    	static uint16_t bat_data1;
    	static uint16_t bat_data2;
#endif
static network_message_t keep_alive_msg;

PROCESS_THREAD(keep_alive_process, ev, data)
{
    PROCESS_BEGIN();
    etimer_set(&keep_alive_timer, keep_alive_interval * CLOCK_SECOND);

    keep_alive_msg.pkttype = network_keep_alive;
    keep_alive_msg.payload.data_len = 5;
    while(1) {
    	PROCESS_WAIT_UNTIL(etimer_expired(&keep_alive_timer));

    	if(keep_alive_interval > 9){
    	    etimer_set(&keep_alive_timer, keep_alive_interval * CLOCK_SECOND);

#ifdef BOARD_LAUNCHPAD_VELA
#if BOARD_LAUNCHPAD_VELA==1
            REP_CAP_mAh = max_17260_sensor.value(MAX_17260_SENSOR_TYPE_REP_CAP);
            AVG_voltage_mV =  max_17260_sensor.value(MAX_17260_SENSOR_TYPE_AVG_V);
            keep_alive_msg.payload.p_data[0] = (uint8_t)(REP_CAP_mAh >> 8);
            keep_alive_msg.payload.p_data[1] = (uint8_t)REP_CAP_mAh;
            keep_alive_msg.payload.p_data[2] = (uint8_t)(AVG_voltage_mV >> 8);
            keep_alive_msg.payload.p_data[3] = (uint8_t)AVG_voltage_mV;
            keep_alive_msg.payload.p_data[4] = trickle_msg.pktnum;
#endif
#else
            bat_data1=0;
            bat_data2=0;
            keep_alive_msg.payload.p_data[0] = (uint8_t)(bat_data1 >> 8);
            keep_alive_msg.payload.p_data[1] = (uint8_t)bat_data1;
            keep_alive_msg.payload.p_data[2] = (uint8_t)(bat_data2 >> 8);
            keep_alive_msg.payload.p_data[3] = (uint8_t)bat_data2;
            keep_alive_msg.payload.p_data[4] = trickle_msg.pktnum;
#endif
            send_to_sink(keep_alive_msg);
    	}else{
            etimer_set(&keep_alive_timer, VERY_LONG_TIMER_VALUE * CLOCK_SECOND); //NOTE: the etimer seems to not reset the expired status, then once stopped the execution keeps looping inside the while(1). Setting it to an high value is a workaround
    	}

    }
    PROCESS_END();
}
