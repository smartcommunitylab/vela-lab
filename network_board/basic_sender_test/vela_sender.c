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
#if OTA
#include "ota-download.h"
#include "ota.h"
#endif
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
static uip_ipaddr_t addr;


static struct trickle_timer tt;
static struct uip_udp_conn *trickle_conn;
static uip_ipaddr_t t_ipaddr;     /* destination: link-local all-nodes multicast */
static struct etimer trickle_et;
network_message_t trickle_msg;

static struct etimer keep_alive_timer;
//static struct etimer rand_delay_timer;

static uint32_t keep_alive_interval = KEEP_ALIVE_INTERVAL;
static struct ctimer m_node_offline_timeout, reboot_delay_timer;

static uint32_t time_between_sends = TIME_BETWEEN_SENDS_DEFAULT_S*CLOCK_SECOND;

static uint8_t send_to_sink (uint8_t *data, int dataSize, pkttype_t pkttype, int sequenceSize );

PROCESS(vela_sender_process, "vela sender process");
PROCESS(trickle_protocol_process, "Trickle Protocol process");
PROCESS(keep_alive_process, "keep alive process");

static void reboot(void *ptr){
  ti_lib_sys_ctrl_system_reset();
}
/*---------------------------------------------------------------------------*/
/* receiver - will be called when this node receives a packet. In vela_sencer 
              we expect the contents to be pieces of a new code image.  These
	      should be sent to the handler that re-assembles the chunks into
	      a code image */
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
  uint8_t message_number=data[2];
  uint16_t pkttype=(((uint16_t)data[0])<<8) + (uint16_t)data[1];
  uint16_t data_offset;
 
#if OTA
  static uint8_t last_received_chunk=0, last_received_subchunk=0;
  static uint16_t datasize;
  if(pkttype==ota_start_of_firmware || pkttype==ota_more_of_firmware || pkttype==ota_end_of_firmware){
      
      uint8_t ota_chunk_no=data[3];
      uint8_t ota_packet_sub_chunk_no=data[4];

      if(pkttype == ota_start_of_firmware) {
        data_offset=5;
        last_received_chunk=0;
        last_received_subchunk=0;
        LOG_INFO("First packet of the firmware received.\n");
        ota_download_init(); //TODO: is this the propper place for this? For sure it must be reinitialized after any firmware update operation (being it correctly executed or not)
      } else if(pkttype == ota_more_of_firmware) {    
        data_offset=5;
      } else if(pkttype == ota_end_of_firmware) {
        data_offset=5;
        LOG_INFO("Last packet of the firmware received.\n");
      }
        
      if(ota_chunk_no!=((last_received_chunk+1)&0xFF)){
        LOG_WARN("ota_chunk_no is not the expected one! Discarding the packet.\n");
        uint8_t ota_data[]={last_received_chunk, 3}; //wrong chunkNo
        send_to_sink(ota_data, sizeof(ota_data), ota_ack, 0);
        return;
      }

      if(ota_packet_sub_chunk_no!=last_received_subchunk+1){
        LOG_WARN("ota_packet_sub_chunk_no is not the expected one! Discarding the packet.\n");
        return; //do not send an ack here! 
      }

      datasize=datalen-data_offset;
      last_received_subchunk=ota_packet_sub_chunk_no;
      LOG_DBG("Firmware packet received. datasize: %u, message_number: %u, ota_chunk_no: %u, ota_packet_sub_chunk_no: %u\n",datasize,message_number,ota_chunk_no,ota_packet_sub_chunk_no);
      /*
      LOG_INFO("packet: 0x ");
      for(uint16_t mmm=0;mmm<datasize;mmm++){
        LOG_INFO_("%02x",data[data_offset+mmm]);
      }
      LOG_INFO_("\n");
      */
      if(ota_download_firmware_subchunk_handler(&data[data_offset],datasize)){ //TODO: add a check on the metadata. If crc_shadow is not 0 refuse the update (only non preverified ota are accepted)
        uint8_t ota_data[]={ota_chunk_no, 0};
        last_received_chunk=ota_chunk_no;
        last_received_subchunk=0;
        if(pkttype == ota_end_of_firmware) {
          last_received_chunk=0;
          last_received_subchunk=0;

          ota_data[1]=verify_ota_slot(active_ota_download_slot); //ota_data[1]==0 everithing ok, ota_data[1]==1 crc ok but not written (the bootloader will do it on the next reboot), ota_data[1]==-3 crc not ok
        }
        send_to_sink(ota_data, sizeof(ota_data), ota_ack, 0);
      }

      LOG_INFO("Firmware packet processed.\n");
      return;
  }
#endif

  if(pkttype==ota_reboot_node){
      data_offset=3;
      //for(uint16_t mmm=0;mmm<datalen;mmm++){
      //  LOG_INFO_("%02x",data[data_offset+mmm]);
      //}
      uint32_t reboot_delay_ms=((uint32_t)data[data_offset])<<24 | ((uint32_t)data[data_offset+1])<<16 | ((uint32_t)data[data_offset+2])<<8 | ((uint32_t)data[data_offset+3]);
      LOG_DBG("Reboot requested with %u ms delay.", (unsigned int)reboot_delay_ms);

      uint32_t delay_ticks=CLOCK_SECOND*reboot_delay_ms/1000;
      LOG_INFO("\nRebooting the node in %u ms.\n", (unsigned int)delay_ticks*1000/CLOCK_SECOND);

      ctimer_set(&reboot_delay_timer, delay_ticks, reboot, NULL);
      return;
  }else {
    LOG_ERR("Unkwown packet type...");
    return;
  }
}
/*---------------------------------------------------------------------------*/
static void offline_timeout_handler(void *ptr){ //when this triggers the node will be reset
  if(NETSTACK_ROUTING.node_is_reachable() && NETSTACK_ROUTING.get_root_ipaddr(&addr)){ //last chance to get the sink address, if it fails the node is reset
    ctimer_restart(&m_node_offline_timeout);
  }else{
    LOG_DBG("The node is offline since more than %d minutes. The node will be reset.\n",NODE_OFFLINE_RESET_TIMEOUT_MINUTES);
    ti_lib_sys_ctrl_system_reset();
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

  //set_global_address();

  simple_udp_register(&unicast_connection, UDP_PORT, NULL, UDP_PORT, receiver);

  process_start(&vela_sender_process, "vela sender process");
  process_start(&trickle_protocol_process, "trickle protocol process");
  process_start(&keep_alive_process, "keep alive process");
  return;
}
/*---------------------------------------------------------------------------*/
static uint8_t send_to_sink (uint8_t *data, int dataSize, pkttype_t pkttype, int sequenceSize ) {

  uint8_t buf[MAX_PACKET_SIZE + APPLICATION_HEADER_SIZE];

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

    simple_udp_sendto(&unicast_connection, buf, totalSize, &addr);  //this will copy the content of buff to another buffer (see uip_udp_packet_send in uip_udp_packet.c)
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
  uip_udp_packet_send(trickle_conn, &trickle_msg, sizeof(trickle_msg)-sizeof(payload_data_t)+trickle_msg.payload.data_len+sizeof(trickle_msg.payload.data_len));
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
          LOG_INFO("Turning Bluetooth off.\n");
          process_post(&cc2650_uart_process, turn_bt_off, NULL);
          break;
	}
	case nordic_turn_bt_on: {
	  ;
	  LOG_INFO("Turning Bluetooth on.\n");
	  process_post(&cc2650_uart_process, turn_bt_on, NULL);
	  break;
	}
	case nordic_turn_bt_on_low: {
	  ;
	  LOG_INFO("Turning Bluetooth on.\n");
	  process_post(&cc2650_uart_process, turn_bt_on_low, NULL);
	  break;
	}
	case nordic_turn_bt_on_def: {
	  ;
	  LOG_INFO("Turning Bluetooth on.\n");
	  process_post(&cc2650_uart_process, turn_bt_on_def, NULL);
	  break;
	}
	case nordic_turn_bt_on_high: {
	  ;
	  LOG_INFO("Turning Bluetooth on.\n");
	  process_post(&cc2650_uart_process, turn_bt_on_high, NULL);
	  break;
	}
        case nordic_turn_bt_on_w_params: {
          ;
          LOG_INFO("Turning Bluetooth on.\n");
          process_post(&cc2650_uart_process, turn_bt_on_w_params, incoming->payload.p_data);
          break;
        }
        case ti_set_keep_alive: {
          ;
          LOG_INFO("Setting keep alive interval to %hhu\n", incoming->payload.p_data[0]);
          PROCESS_CONTEXT_BEGIN(&keep_alive_process);
          keep_alive_interval = incoming->payload.p_data[0];
          if(keep_alive_interval > 9){
            etimer_set(&keep_alive_timer, 0); //setting the timer to 0 makes it send the keep alive immediately, on the next loop the timer will use keep_alive_interval instead
          }else{
            etimer_set(&keep_alive_timer, VERY_LONG_TIMER_VALUE * CLOCK_SECOND);
            LOG_INFO("Not a valid interval, turning keep alive off\n");
          }
          PROCESS_CONTEXT_END(&keep_alive_process);
          break;
        }
        case ti_set_batt_info_int:  //deprecated
          break;
          /*{
          ;
          if(incoming->payload.p_data[0] != 0){
            LOG_INFO("Enabling battery info report with interval %u\n",incoming->payload.p_data[0]);
          }else{
            LOG_INFO("Disabling battery info report\n");
          }
          process_post(&vela_node_process, set_battery_info_interval, incoming->payload.p_data);
          break;
        }*/
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

  //event_buffer_empty = process_alloc_event();
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
          if (sequenceSize > MAX_REPORTS_PACKET_SIZE) {
            if(offset==0) { // more than one packet to send, this is the first
              ret = send_to_sink (&eventData->p_data[0], MAX_REPORTS_PACKET_SIZE,
                                  network_new_sequence, sequenceSize );

            } else { // this is not the first or last packet of a sequence
              ret = send_to_sink(&eventData->p_data[offset], MAX_REPORTS_PACKET_SIZE,
                                 network_active_sequence, 0 );
            }
	        sequenceSize -= MAX_REPORTS_PACKET_SIZE;
	        offset += MAX_REPORTS_PACKET_SIZE;

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
      //if(ev == event_bat_data_ready) {
      //  LOG_INFO("Received battery data event\n");
      //  send_to_sink((uint8_t*)data, 12, network_bat_data, 0);
        /*        bat_message.pkttype = network_bat_data;
		  memcpy(bat_message.payload.p_data, data, 12);
		  bat_message.payload.data_len = 12;
		  send_to_sink(bat_message);*/
      //}
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

/* create the list of neighbors, placed in the parameter neighborBuf.  Return the amount of data put into the buffer */
static uint16_t prepNeighborList(uint8_t* neighborBufUint8) {
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
  return index;
}

PROCESS_THREAD(keep_alive_process, ev, data)
{
  PROCESS_BEGIN();
  etimer_set(&keep_alive_timer, keep_alive_interval * CLOCK_SECOND);

  while(1) {

    clock_time_t keep_alive_timer_value=0;

    PROCESS_WAIT_UNTIL(etimer_expired(&keep_alive_timer));

    if(keep_alive_interval > 9){

        uint16_t REP_CAP_mAh=0, REP_SOC_permillis=0, TTE_minutes=0, AVG_voltage_mV=0;
        int16_t AVG_current_100uA=0, AVG_temp_10mDEG=0;
        uint8_t keep_alive_data[100]; // ALM: increaated from 5 to 100 to include neighbor information
        uint16_t c=0;

#ifdef BOARD_LAUNCHPAD_VELA
#if BOARD_LAUNCHPAD_VELA==1
        REP_CAP_mAh = max_17260_sensor.value(MAX_17260_SENSOR_TYPE_REP_CAP);
        REP_SOC_permillis = max_17260_sensor.value(
		                     MAX_17260_SENSOR_TYPE_REP_SOC);
        TTE_minutes = max_17260_sensor.value(MAX_17260_SENSOR_TYPE_TTE);
        AVG_current_100uA = max_17260_sensor.value(MAX_17260_SENSOR_TYPE_AVG_I);
        AVG_voltage_mV = max_17260_sensor.value(
		                  MAX_17260_SENSOR_TYPE_AVG_V);
        AVG_temp_10mDEG = max_17260_sensor.value(MAX_17260_SENSOR_TYPE_AVG_TEMP);
        LOG_DBG("Remaining battery capacity = %d mAh ", REP_CAP_mAh);
        LOG_DBG_("or %d.%d %%. ", (uint16_t) (REP_SOC_permillis / 10), REP_SOC_permillis % 10);
        LOG_DBG_("Remaining runtime with this consumption: %d hours and %d minutes. ", TTE_minutes / 60, TTE_minutes % 60);
        if (AVG_current_100uA > 0) {
            LOG_DBG_("Avg current %d.%d mA ", AVG_current_100uA / 10, AVG_current_100uA % 10);
        } else {
            LOG_DBG_("Avg current -%d.%d mA ", -AVG_current_100uA / 10, -AVG_current_100uA % 10);
        }
        LOG_DBG_("Avg voltage = %d mV ", AVG_voltage_mV);
        if (AVG_temp_10mDEG > 0)
        {
          LOG_DBG_("Avg temp %d.%d °C ", AVG_temp_10mDEG / 100, AVG_temp_10mDEG % 100);
        }
        else
        {
          LOG_DBG_("Avg temp %d.%d °C ", -AVG_temp_10mDEG / 100, -AVG_temp_10mDEG % 100);
        }
        LOG_DBG_("\n");
#endif
#else
        REP_CAP_mAh++; REP_SOC_permillis++; TTE_minutes++; AVG_voltage_mV++; AVG_current_100uA++; AVG_temp_10mDEG++;
#endif
        //STORE BATTERY DATA IN KEEPALIVE PAYLOAD
        keep_alive_data[c++] = (uint8_t)(REP_CAP_mAh >> 8);                             //0
        keep_alive_data[c++] = (uint8_t)REP_CAP_mAh;                                    //1
        keep_alive_data[c++] = (uint8_t)(REP_SOC_permillis >> 8);                       //2
        keep_alive_data[c++] = (uint8_t)REP_SOC_permillis;                              //3
        keep_alive_data[c++] = (uint8_t)(TTE_minutes >> 8);                             //4
        keep_alive_data[c++] = (uint8_t)TTE_minutes;                                    //5
        keep_alive_data[c++] = (uint8_t)(AVG_current_100uA >> 8);                       //6
        keep_alive_data[c++] = (uint8_t)AVG_current_100uA;                              //7
        keep_alive_data[c++] = (uint8_t)(AVG_voltage_mV >> 8);                          //8
        keep_alive_data[c++] = (uint8_t)AVG_voltage_mV;                                 //9
        keep_alive_data[c++] = (uint8_t)(AVG_temp_10mDEG >> 8);                         //10
        keep_alive_data[c++] = (uint8_t)AVG_temp_10mDEG;                                //11

#if OTA
        OTAMetadata_t runningFirmware_metadata;
        get_current_metadata(&runningFirmware_metadata);

        //STORE RUNNING FIRMWARE METADATA IN KEEPALIVE PAYLOAD
        keep_alive_data[c++] = (uint8_t)(runningFirmware_metadata.crc >> 8);            //12
        keep_alive_data[c++] = (uint8_t)runningFirmware_metadata.crc;                   //13
        keep_alive_data[c++] = (uint8_t)(runningFirmware_metadata.crc_shadow >> 8);     //14
        keep_alive_data[c++] = (uint8_t)runningFirmware_metadata.crc_shadow;            //15
        keep_alive_data[c++] = (uint8_t)(runningFirmware_metadata.size >> 24);          //16
        keep_alive_data[c++] = (uint8_t)(runningFirmware_metadata.size >> 16);          //17
        keep_alive_data[c++] = (uint8_t)(runningFirmware_metadata.size >> 8);           //18
        keep_alive_data[c++] = (uint8_t)runningFirmware_metadata.size;                  //19
        keep_alive_data[c++] = (uint8_t)(runningFirmware_metadata.uuid >> 24);          //20
        keep_alive_data[c++] = (uint8_t)(runningFirmware_metadata.uuid >> 16);          //21
        keep_alive_data[c++] = (uint8_t)(runningFirmware_metadata.uuid >> 8);           //22
        keep_alive_data[c++] = (uint8_t)runningFirmware_metadata.uuid;                  //23
        keep_alive_data[c++] = (uint8_t)(runningFirmware_metadata.version >> 8);        //24
        keep_alive_data[c++] = (uint8_t)runningFirmware_metadata.version;               //25
#else   
        keep_alive_data[c++] = 0;                                                       //12
        keep_alive_data[c++] = 0;                                                       //13
        keep_alive_data[c++] = 0;                                                       //14
        keep_alive_data[c++] = 0;                                                       //15
        keep_alive_data[c++] = 0;                                                       //16
        keep_alive_data[c++] = 0;                                                       //17
        keep_alive_data[c++] = 0;                                                       //18
        keep_alive_data[c++] = 0;                                                       //19
        keep_alive_data[c++] = 0;                                                       //20
        keep_alive_data[c++] = 0;                                                       //21
        keep_alive_data[c++] = 0;                                                       //22
        keep_alive_data[c++] = 0;                                                       //23
        keep_alive_data[c++] = 0;                                                       //24
        keep_alive_data[c++] = 0;                                                       //25
#endif
        keep_alive_data[c++] = trickle_msg.pktnum;                                      //26

      // add the neighbor table information to the keepalive packet
      uint16_t neighborLength=0; // track the size of the neighbor table in the packet
      neighborLength = prepNeighborList(&keep_alive_data[c]);

      ///////debugging  vvvvvvvv
      LOG_INFO("neighbor size=%d Data:",neighborLength); 
      for (uint16_t i=0; i<neighborLength;i++)
	     LOG_INFO_("%c",(char)keep_alive_data[c+i]);
      LOG_INFO_("\n");
      ///////debugging ^^^^^^^^

      // need to send the 5 bytes of the standard keepalive PLUS the lenght of the neighbor table      
      send_to_sink(keep_alive_data, c+neighborLength, network_keep_alive,0);
      keep_alive_timer_value=keep_alive_interval * CLOCK_SECOND;

    }else{
      keep_alive_timer_value=VERY_LONG_TIMER_VALUE * CLOCK_SECOND; //NOTE: the etimer seems to not reset the expired status, then once stopped the execution keeps looping inside the while(1). Setting it to an high value is a workaround
    }

    etimer_set(&keep_alive_timer, keep_alive_timer_value);

  }
  PROCESS_END();
}
