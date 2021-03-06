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
#include "rpl-neighbor.h" // added for neighbor reporting
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
//#define LOG_LEVEL LOG_LEVEL_NONE

#ifdef BOARD_LAUNCHPAD_VELA
#if BOARD_LAUNCHPAD_VELA==1
#include "max-17260-sensor.h"
#endif
#endif

#define VERY_LONG_TIMER_VALUE   14510024
#define MAX_RANDOM_DELAY_S      0.5f
#define NODE_OFFLINE_RESET_TIMEOUT_MINUTES  30
#define NODE_OFFLINE_RESET_TIMEOUT CLOCK_SECOND*60*NODE_OFFLINE_RESET_TIMEOUT_MINUTES //30 minutes timeout. If the nodes remains offline for more than this timeout, it will be reset

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
static struct ctimer m_node_offline_timeout;

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
static void offline_timeout_handler(void *ptr){ //when this triggers the node will be reset
  if(NETSTACK_ROUTING.node_is_reachable() && NETSTACK_ROUTING.get_root_ipaddr(&addr)){ //last chance to get the sink address, if it fails the node is reset
    ctimer_restart(&m_node_offline_timeout);
  }else{
    LOG_DBG("The node is offline since more than %d minutes. The node will be reset.\n",NODE_OFFLINE_RESET_TIMEOUT_MINUTES);
    while(1){};
  }
}
/*---------------------------------------------------------------------------*/
void vela_sender_init() {
  random_init(0);

  LOG_INFO("vela_sender: initializing \n");
  message_number = 0;
  ctimer_set(&m_node_offline_timeout, NODE_OFFLINE_RESET_TIMEOUT, offline_timeout_handler, NULL);
  LOG_INFO("A timer will reset the node if it stays offline for more than %d minutes\n",NODE_OFFLINE_RESET_TIMEOUT_MINUTES);
  //    servreg_hack_init();

  set_global_address();

  simple_udp_register(&unicast_connection, UDP_PORT, NULL, UDP_PORT, receiver);

  process_start(&vela_sender_process, "vela sender process");
  process_start(&trickle_protocol_process, "trickle protocol process");
  process_start(&keep_alive_process, "keep alive process");
  return;
}
/*---------------------------------------------------------------------------*/
static uint8_t send_to_sink (uint8_t *data, int dataSize, pkttype_t pkttype, int sequenceSize ) {

  // note that unless this is the start of a sequence of packets, sequenceSize will be ignored

  leds_toggle(LEDS_GREEN);
  if(NETSTACK_ROUTING.node_is_reachable() && NETSTACK_ROUTING.get_root_ipaddr(&addr)){
    LOG_DBG("sending to sink...\n");
    ctimer_restart(&m_node_offline_timeout);
    uint8_t totalSize = 0;
    message_number++;
    buf[0] = pkttype >> 8;
    buf[1] = pkttype;
    buf[2] = message_number;
    if (pkttype == network_new_sequence) { // the first message of a sequence must contain its length
      buf[3] = (uint8_t)(sequenceSize >> 8); // add the size of the whole sequence
      buf[4] = (uint8_t)sequenceSize;
      memcpy(&buf[5], data, dataSize);
      totalSize = dataSize+5;
      LOG_DBG("Sending size: %u, msgNum: %u ,first 2 byte: 0x%02x 0x%02x to ",
              totalSize, message_number, buf[5], buf[6]);
    } else { // this is not the first message of a sequence, so it starts w/ actual data
      memcpy(&buf[3], data, dataSize);
      totalSize = dataSize+3;
      LOG_DBG("Sending size: %u, msgNum: %u ,first 2 byte: 0x%02x 0x%02x to ",
              totalSize, message_number, buf[3], buf[4]);
    }

    LOG_DBG_6ADDR(&addr);
    LOG_DBG_("\n");

    simple_udp_sendto(&unicast_connection, buf, totalSize, &addr);
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

static void node_delay(void){ //not required with the new contiki and tsch
  //    uint32_t delayTime=(MAX_RANDOM_DELAY_S*CLOCK_SECOND*(node_id & (0x00FF)))/256;
  //uint32_t delayTime=(random_rand()*MAX_RANDOM_DELAY_S*CLOCK_SECOND)/0xFFFF;
  //LOG_DBG("node_id 0x%4X, PACKET Delay %lu\n",node_id, delayTime);
  //clock_wait(delayTime);
  //Commented the more appropriate wait implementation. It doesn't work because of some problems with context switch
  //clock_wait() is enough since it is only unatantum
  //    PROCESS_CONTEXT_BEGIN(trickle_protocol_process);
  //    etimer_set(&rand_delay_timer, delayTime);
  //    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&rand_delay_timer));
  //    PROCESS_CONTEXT_END(trickle_protocol_process);
  return;
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
          //process_post(&vela_node_process, set_battery_info_interval, incoming->payload.p_data);
          break;
        }
        case network_set_time_between_sends: {
          uint16_t time_between_sends_ms = (incoming->payload.p_data[0]<<8) | incoming->payload.p_data[1];
          time_between_sends = time_between_sends_ms*(CLOCK_SECOND/1000.0);
          LOG_INFO("Setting Time Between Sends to %d ms, %lu ticks\n",time_between_sends_ms,time_between_sends);
          break;
        }
        case nordic_ble_tof_enable: {
        ;
          if(incoming->payload.data_len >0){
            process_post(&cc2650_uart_process, turn_ble_tof_onoff, incoming->payload.p_data);
          }
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

//static network_message_t response;
//static struct etimer pause_timer;
static data_t* eventData;
static int offset = 0;
static int sequenceSize = 0;
//static network_message_t chunk;
static uint8_t ret;
//static uint8_t* temp;
//static network_message_t bat_message;

PROCESS_THREAD(vela_sender_process, ev, data) {
  PROCESS_BEGIN();
  NETSTACK_MAC.on();
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
        sequenceSize = (int)eventData->data_len; // amount of data in the buffer not yet sent
        while ( sequenceSize > 0 ) {
          if (sequenceSize > MAX_PACKET_SIZE) {
            if(offset==0) { // more than one packet to send, this is the first
              ret = send_to_sink (&eventData->p_data[0], MAX_PACKET_SIZE,
                                  network_new_sequence, sequenceSize );

            } else { // this is not the first or last packet of a sequence
              ret = send_to_sink(&eventData->p_data[offset], MAX_PACKET_SIZE,
                                 network_active_sequence, 0 );
            }
	    sequenceSize -= MAX_PACKET_SIZE;
	    offset += MAX_PACKET_SIZE;

            etimer_set(&periodic_timer, time_between_sends);
            PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));
          } else { // this is the first and last packet of a seuqnce
            if (offset == 0) {
              ret = send_to_sink (&eventData->p_data[0], sequenceSize,
                                  network_new_sequence, sequenceSize);
            } else { // this is the last packet, but not the first.
              // this is my last packet
              ret = send_to_sink (&eventData->p_data[offset], sequenceSize,
                                  network_last_sequence, sequenceSize);

            }
	    sequenceSize = 0;

          }
        }
      }
      if(ev == event_pong_received){
        LOG_INFO("Sending pong\n");
        send_to_sink((uint8_t*)data, 1, network_respond_ping, 0);
        /*temp = (uint8_t*)data;
        response.pkttype = network_respond_ping;
        response.payload.data_len = 1;
        response.payload.p_data[0] = temp[0];
        send_to_sink(response); */
      }
      if(ev == event_bat_data_ready) {
        LOG_INFO("Received battery data event\n");
        send_to_sink((uint8_t*)data, 12, network_bat_data, 0);
        /*        bat_message.pkttype = network_bat_data;
        memcpy(bat_message.payload.p_data, data, 12);
        bat_message.payload.data_len = 12;
        send_to_sink(bat_message);*/
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
//static network_message_t keep_alive_msg;
static uint8_t keep_alive_data[200];

/* create the list of neighbors, placed in the parameter neighborBuf.  Return the amount of data put into the buffer */
static int prepNeighborList(uint8_t* neighborBufUint8) {
  LOG_INFO("prepNeighborList***********************************************************************************************************************\n");
  int index = 0;
  char* neighborBuf = (char *)neighborBufUint8;
  static char tmpBuf[10];
  int addressLen = 0;
  //LOG_INFO("index at the beginning %d\n",index);
  if(NETSTACK_ROUTING.node_is_reachable() && NETSTACK_ROUTING.get_root_ipaddr(&addr)){
    static int buflen = 195; // needs to be the size of the keep_alive_data buffer
    if(curr_instance.used) {

      addressLen = log_6addr_compact_snprint(tmpBuf, 10, rpl_get_global_address()); // insert the ID of this node
      neighborBuf[index] = tmpBuf[addressLen-1];
      index++;
	    
      //index += log_6addr_compact_snprint(neighborBuf, buflen, rpl_get_global_address()); // insert the ID of this node
      if (curr_instance.dag.rank == RPL_INFINITE_RANK) {
	index += snprintf(neighborBuf+index, buflen-index, "i");  // error rank if not yet set for this node
      } else {
	index += snprintf(neighborBuf+index, buflen-index, "%5u",curr_instance.dag.rank);  // rank of this node
      }
      
      // start cycling through the neighbors
      rpl_nbr_t *nbr = nbr_table_head(rpl_neighbors); // get the head of the neighbor table
      

      while(nbr != NULL) {
	if (rpl_neighbor_is_parent(nbr)) {  // identify as potential parent, or just neighbor
	  index += snprintf(neighborBuf+index, buflen-index, " P"); // neighbor is a potential parent 
	} else {
	  index += snprintf(neighborBuf+index, buflen-index, " N"); // neighbor is NOT a potential parent
	}
	if (curr_instance.dag.preferred_parent == nbr) { // identify as the current parent or not
	  index += snprintf(neighborBuf+index, buflen-index, "*:"); // neighbor is the curent parent
	}  else {
	  index += snprintf(neighborBuf+index, buflen-index, ":"); // neighbor is NOT the current parent
	}

	addressLen = log_6addr_compact_snprint(tmpBuf, 10, rpl_neighbor_get_ipaddr(nbr)); // get the ID of the neighbor
	neighborBuf[index] = tmpBuf[addressLen-1]; // insert neighbor's id
	index++;
	//index += log_6addr_compact_snprint(neighborBuf+index, buflen-index, rpl_neighbor_get_ipaddr(nbr)); // insert ID of the neighbor
	
	index += snprintf(neighborBuf+index, buflen-index, "%5u",nbr->rank);  // rank of the neighbor

	nbr = nbr_table_next(rpl_neighbors, nbr);
      }

      
    }
  }
  //  LOG_INFO("returning--------------------%s\n",neighborBuf);
  return index;
}


PROCESS_THREAD(keep_alive_process, ev, data)
{
  PROCESS_BEGIN();
  etimer_set(&keep_alive_timer, keep_alive_interval * CLOCK_SECOND);

  //keep_alive_msg.pkttype = network_keep_alive;
  //keep_alive_msg.payload.data_len = 5;
  while(1) {
    PROCESS_WAIT_UNTIL(etimer_expired(&keep_alive_timer));

    if(keep_alive_interval > 9){
      etimer_set(&keep_alive_timer, keep_alive_interval * CLOCK_SECOND);

#ifdef BOARD_LAUNCHPAD_VELA
#if BOARD_LAUNCHPAD_VELA==1
      REP_CAP_mAh = max_17260_sensor.value(MAX_17260_SENSOR_TYPE_REP_CAP);
      AVG_voltage_mV =  max_17260_sensor.value(MAX_17260_SENSOR_TYPE_AVG_V);
      keep_alive_data[0] = (uint8_t)(REP_CAP_mAh >> 8);
      keep_alive_data[1] = (uint8_t)REP_CAP_mAh;
      keep_alive_data[2] = (uint8_t)(AVG_voltage_mV >> 8);
      keep_alive_data[3] = (uint8_t)AVG_voltage_mV;
      keep_alive_data[4] = trickle_msg.pktnum;

      /*            keep_alive_msg.payload.p_data[0] = (uint8_t)(REP_CAP_mAh >> 8);
      keep_alive_msg.payload.p_data[1] = (uint8_t)REP_CAP_mAh;
      keep_alive_msg.payload.p_data[2] = (uint8_t)(AVG_voltage_mV >> 8);
      keep_alive_msg.payload.p_data[3] = (uint8_t)AVG_voltage_mV;
      keep_alive_msg.payload.p_data[4] = trickle_msg.pktnum;*/

#endif
#else
      bat_data1=0;
      bat_data2=0;
      keep_alive_data[0] = (uint8_t)(bat_data1 >> 8);
      keep_alive_data[1] = (uint8_t)bat_data1;
      keep_alive_data[2] = (uint8_t)(bat_data2 >> 8);
      keep_alive_data[3] = (uint8_t)bat_data2;
      keep_alive_data[4] = trickle_msg.pktnum;

      /*      keep_alive_msg.payload.p_data[0] = (uint8_t)(bat_data1 >> 8);
      keep_alive_msg.payload.p_data[1] = (uint8_t)bat_data1;
      keep_alive_msg.payload.p_data[2] = (uint8_t)(bat_data2 >> 8);
      keep_alive_msg.payload.p_data[3] = (uint8_t)bat_data2;
      keep_alive_msg.payload.p_data[4] = trickle_msg.pktnum; */
#endif
      // sending the neighbor table in the keepalive message
      static int neighborLength=0;
      neighborLength = prepNeighborList(&keep_alive_data[5]);  // put the neighbor table at the end of the keepalive message

      ///////debugging  vvvvvvvv
      LOG_INFO("neighbor size=%d      Data:",neighborLength); 
      for (int i=5; i<neighborLength;i++) 
	LOG_INFO_("%c",(char)keep_alive_data[i]);
      LOG_INFO_("\n");
      ///////debugging ^^^^^^^^

      
      send_to_sink(keep_alive_data, neighborLength+5, network_keep_alive,0); // add 5 because of the data added to the keep_alive message above
    }else{
      etimer_set(&keep_alive_timer, VERY_LONG_TIMER_VALUE * CLOCK_SECOND); //NOTE: the etimer seems to not reset the expired status, then once stopped the execution keeps looping inside the while(1). Setting it to an high value is a workaround
    }

  }
  PROCESS_END();
}
