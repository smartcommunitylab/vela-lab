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
#include "sink_receiver.h"
#include "ti-lib.h"

#include "sys/log.h"
#define LOG_MODULE "sink_receiver"
#define LOG_LEVEL LOG_LEVEL_DBG

#define TO_BYTE(hex_data) (((hex_data ) <= '9') ? ((hex_data)  - '0') : ((hex_data)  - 'A' + 10))
#define TO_HEX(byte_data) (((byte_data) <=  9 ) ? ((byte_data) + '0') : ((byte_data) + 'A' - 10)) //NOTE THAT THIS PROPERLY WORKS ONLY ON 4 BITS!!!
#define UART_PKT_START_SYMBOL           '\x02'      //start of text - this is the same as in uart_util.h

static struct simple_udp_connection unicast_connection;
static uint8_t message_number = 0;
//static uint8_t buf[MAX_PACKET_SIZE + 10];

static struct etimer periodic_timer;

static struct trickle_timer tt;
static struct uip_udp_conn *trickle_conn;
static uip_ipaddr_t t_ipaddr;     /* destination: link-local all-nodes multicast */
static struct etimer trickle_et;
//static struct etimer periodicSinkOutput;

static network_message_t* incoming;
static network_message_t trickle_msg;

//static uint8_t lost; // bookkeeping
//#define MAX_NETWORK_SIZE  10
//static uint8_t lastFrom[MAX_NETWORK_SIZE]; // bookkeeping


#define OTA_CHUNK_SIZE   256

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
static uint8_t send_to_node (uint8_t *data, uint16_t dataSize, uint8_t *prefix, uint8_t prefix_len, pkttype_t pkttype, uip_ipaddr_t *dest) {

  uint8_t buf[MAX_PACKET_SIZE + APPLICATION_HEADER_SIZE];

  leds_toggle(LEDS_GREEN);

  if(NETSTACK_ROUTING.node_is_reachable() ) {
    LOG_DBG("sending to node...\n");
    int totalSize = 0;
    uint16_t idx=0;
    message_number++;
    buf[idx++] = pkttype >> 8;
    buf[idx++] = pkttype;
    buf[idx++] = message_number;

    if(prefix_len>0 && prefix!=NULL){ //there is a prefix for this packet, then add it
        memcpy(&buf[idx], prefix, prefix_len);
        idx+=prefix_len;
    }

    /*LOG_INFO("packet: 0x");
    for(uint16_t mmm=0;mmm<dataSize;mmm++){
      LOG_INFO_("%02x",data[mmm]);
    }
    LOG_INFO_("\n");
    */
    memcpy(&buf[idx], data, dataSize);
    idx+=dataSize;
    totalSize=idx;
    
    LOG_DBG_6ADDR(dest);
    LOG_DBG_("\n");

    simple_udp_sendto(&unicast_connection, buf, totalSize, dest); //this will copy the content of buff to another buffer (see uip_udp_packet_send in uip_udp_packet.c)
    leds_on(LEDS_RED);
    return 0;
  } else {
    leds_off(LEDS_RED);
    LOG_WARN("ERROR: No sink available!\n");
    return -1;
  }
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


  LOG_INFO("received from ");
  LOG_INFO_6ADDR(sender_addr);
  LOG_INFO_(" size=%d  num=%d\n",datalen,data[2]);

  cc26xx_uart_write_byte(UART_PKT_START_SYMBOL);

  for (uint16_t i=0;i<16;i++){
    sendCharAsHexString(sender_addr->u8[i]);
  }
  //sendCharAsHexString(sender_addr->u8[15]);
  for (uint16_t i=0;i<datalen;i++){
    sendCharAsHexString(data[i]);
  }
  cc26xx_uart_write_byte('\n');
/*
  static uint8_t prev = 0;
  if (sender_addr->u8[15] <= MAX_NETWORK_SIZE) {
    prev = lastFrom[sender_addr->u8[15]]; // prev is last msg id from this node
    lastFrom[sender_addr->u8[15]] = data[2]; // set the last msg id from this node.

    if (prev+1 != data[2]) {
      if (prev == 255 && data[2]==1) {
	// all ok
      } else {
	    printf("*****************************LOST DATA FROM NODE %d\n",sender_addr->u8[15]);
	    lost ++;
      }
    }
  }*/
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
  uip_udp_packet_send(trickle_conn, &trickle_msg, sizeof(trickle_msg)-sizeof(payload_data_t)+trickle_msg.payload.data_len+sizeof(trickle_msg.payload.data_len));
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

//static struct etimer uping;



static uint8_t ret;

static codeChunk_t chunk;
static uint8_t chunkBuffer[OTA_CHUNK_SIZE];
static uip_ipaddr_t chunkDesitnation;
// for fake, fill the buffer with the fake data to be sent
#define OTA_PACKET_PREFIX_SIZE  2 //The prefix will contain chunk_no and sub_chunk_packet_no, 2 bytes are enough
PROCESS_THREAD(sink_receiver_process, ev, data)
{
    PROCESS_BEGIN();

    simple_udp_register(&unicast_connection, UDP_PORT, NULL, UDP_PORT, receiver);

    NETSTACK_ROUTING.root_start();
    NETSTACK_MAC.on();


    static codeChunk_t *chunkData;
    static uint16_t payload_size;
    static uint16_t pkt_type;
    static uint16_t offset = 0;
    static uint16_t bytesRemaining = 0;
    static uint8_t sub_chunk_packet_no;
    static uint8_t prefix_buff[OTA_PACKET_PREFIX_SIZE];

    while(1) {

        PROCESS_WAIT_EVENT();

        if (ev == event_code_ready) {
            chunkData = (codeChunk_t*)data; // the code chunk to send

            offset=0; // offset from the begining of the buffer to send next, starts at 0
            bytesRemaining = chunkData->data_len; // amount of data in the buffer not yet sent
            sub_chunk_packet_no=1;

            while ( bytesRemaining > 0 ) {
                prefix_buff[0]=chunkData->chunk_no;
                prefix_buff[1]=sub_chunk_packet_no++;
                
                LOG_DBG("bytesRemaining: %u, offset: %u, is_first: %u, is_last: %u, chunk_no: %u, sub_chunk_packet_no: %u\n",bytesRemaining,offset,chunkData->is_first,chunkData->is_last,prefix_buff[0],prefix_buff[1]);

                if (bytesRemaining >= MAX_PACKET_SIZE) {
                    if(offset==0 && chunkData->is_first) {            // this is the first packet of the first chunk of the firmware
                        pkt_type=ota_start_of_firmware;
                        LOG_DBG("First firmware packet is going to be sent.\n");
                    } else {                                          //not the first nor the last packet of the firmware
                        pkt_type=ota_more_of_firmware;
                    } 

                    if(chunkData->is_last && bytesRemaining==MAX_PACKET_SIZE){ // this is the last packet, and the firmware size is exactly a multiple of MAX_PACKET_SIZE (pretty rare condition, but it can happen)
                        pkt_type=ota_end_of_firmware;
                    } 

                    payload_size=MAX_PACKET_SIZE;
           

                    ret = send_to_node (&chunkData->p_data[offset], payload_size, prefix_buff, OTA_PACKET_PREFIX_SIZE, pkt_type, chunkData->p_destination);

                    bytesRemaining -= MAX_PACKET_SIZE;
                    offset += MAX_PACKET_SIZE;
                } else {

                    if(chunkData->is_last && !chunkData->is_first){        // this is the last packet of the last chunk of the firmware
                        pkt_type=ota_end_of_firmware;
                        sub_chunk_packet_no=0;
                    //}else if(!chunkData->is_last && !chunkData->is_first){        //not the first nor the last packet of the firmware
                    }else if(!chunkData->is_last && (!chunkData->is_first || offset!=0)){        //not the first nor the last packet of the firmware
                        pkt_type=ota_more_of_firmware;
                    } else { //equivalent to if(chunkData->is_first).     // this means that the first firmware chunk is smaller than MAX_PACKET_SIZE. Which is pretty impossible
                        LOG_ERR("Wrong packet data, cannot send! Err 2\n");
                        break; 
                    }

                    payload_size=bytesRemaining;
                    bytesRemaining = 0;

                    ret = send_to_node (&chunkData->p_data[offset], payload_size,  prefix_buff, OTA_PACKET_PREFIX_SIZE, pkt_type, chunkData->p_destination);
                }

                if(bytesRemaining!=0){
                    LOG_INFO("WAITING ON TIMER\n");
                    if(pkt_type==ota_start_of_firmware){ //if it is the first packet of the firmware, leave more time for the node to erase the flash
                        etimer_set(&periodic_timer, 10*CLOCK_SECOND);
                    }else{
                        etimer_set(&periodic_timer, CLOCK_SECOND/5);
                    }
                    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));
                }
            }
        }
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
static uip_ipaddr_t outgoing_network_message_ipaddr;


uint8_t is_broadcast(uip_ipaddr_t *addr_to_check){
    return addr_to_check->u8[0]==0xFF && addr_to_check->u8[1]==0xFF && addr_to_check->u8[2]==0xFF && addr_to_check->u8[3]==0xFF && addr_to_check->u8[4]==0xFF && addr_to_check->u8[5]==0xFF && addr_to_check->u8[6]==0xFF && addr_to_check->u8[7]==0xFF && addr_to_check->u8[8]==0xFF && addr_to_check->u8[9]==0xFF && addr_to_check->u8[10]==0xFF && addr_to_check->u8[11]==0xFF && addr_to_check->u8[12]==0xFF && addr_to_check->u8[13]==0xFF && addr_to_check->u8[14]==0xFF && addr_to_check->u8[15]==0xFF;
}

PROCESS_THREAD(uart_reader_process, ev, data)
{
  PROCESS_BEGIN();
  cc26xx_uart_init();
  serial_line_init();
  cc26xx_uart_set_input(serial_line_input_byte);
  //etimer_set(&periodicSinkOutput,CLOCK_SECOND*15);

  //for (uint8_t i=0; i<MAX_NETWORK_SIZE; i++) lastFrom[i]=0;// bookkeeping
  //lost = 0;// bookkeeping

  chunk.p_data = chunkBuffer; //TODO: chunk and chunkData structs might be unified, the large buffer is only one then there is not much memory save therefore for now I leave it 
  chunk.p_destination = &chunkDesitnation;
  chunk.is_first=0;
  chunk.is_last=0;

  while(1) {
    PROCESS_YIELD();
    if(ev == serial_line_event_message){
      
      //LOG_INFO("serial_line_event_message\n");
      input=(uint8_t*)data;
      serial_line_len = strlen((char*)input);

      if(serial_line_len>=2+NET_MESS_MSGADDR_LEN*2 && serial_line_len<SERIAL_LINE_CONF_BUFSIZE){
	    for(uint8_t ci=0;ci<serial_line_len/2;ci++){
	      byte_array[ci] = ((0x0F & TO_BYTE(input[2*ci])) << 4) + ( 0x0F & TO_BYTE(input[2*ci + 1]));
	    }

        uint8_t idx=0;
        uip_ip6addr_u8(&outgoing_network_message_ipaddr,byte_array[0],byte_array[1],byte_array[2],byte_array[3],byte_array[4],byte_array[5],byte_array[6],byte_array[7],byte_array[8],byte_array[9],byte_array[10],byte_array[11],byte_array[12],byte_array[13],byte_array[14],byte_array[15]);
        idx+=16;

	    payload_size=byte_array[idx++];
        
        
        uint8_t cs=0;                
        for(uint16_t ii=0;ii<payload_size;ii++){
            cs+=byte_array[idx+ii];
        }
        if(cs!=byte_array[payload_size]){
            LOG_WARN("CRC ERROR (calculated %d, received %d), discarding the serial packet!\n",cs,byte_array[payload_size]);
            continue;
        }
        
        if(is_broadcast(&outgoing_network_message_ipaddr)){

	        trickle_msg.pkttype = ((uint16_t) byte_array[idx++]) << 8;
	        trickle_msg.pkttype |= byte_array[idx++];
	        trickle_msg.pktnum++;
	        trickle_msg.payload.data_len=payload_size;
	        memcpy(trickle_msg.payload.p_data, &byte_array[NET_MESS_MSGADDR_LEN+NET_MESS_MSGTYPE_LEN+NET_MESS_MSGLEN_LEN], payload_size);

            LOG_INFO("Trickle change. Counter: %u, pkttype: 0x%04x, payload.data_len: 0x%02x\n",trickle_msg.pktnum, trickle_msg.pkttype,trickle_msg.payload.data_len);
//            for(uint8_t i = 0; i < payload_size; i++){
//                LOG_INFO("payload[%u] 0x%02x",i,trickle_msg.payload.p_data[i]);
//            }
//            LOG_INFO("\n");
	        
            trickle_timer_reset_event(&tt);
            
	        leds_toggle(LEDS_RED);
        }else{
            static uint16_t subchunk_offset = 0;
            uint16_t pkttype;
            static uint8_t is_first=0, is_last=0, last_received_subchunk=0;

            pkttype = ((uint16_t) byte_array[idx++]) << 8;
	        pkttype |= byte_array[idx++];

            if(pkttype==ota_start_of_firmware || pkttype==ota_more_of_firmware || pkttype==ota_end_of_firmware){ //using all these packet types might be reduntand, but it adds a very little cost and it makes the transfer safer

                uint8_t chunk_no=byte_array[idx++];
                uint8_t sub_chunk_no=byte_array[idx++];
                uint8_t sub_chunk_data_size=payload_size-2; //-2 because the first two bytes are chunk_no and sub_chunk_no

                if(pkttype==ota_start_of_firmware){
                    is_first=1; //is first subchunk of the first chunk of the firmware?
                    subchunk_offset=0;
                    LOG_DBG("First firmware subchunk received. chunk_no: %u, sub_chunk_no: %u\n",chunk_no, sub_chunk_no);
                }else if(pkttype==ota_end_of_firmware){                    
                    is_last=1; //is last subchunk of the chunk of the firmware?
                    LOG_DBG("Last firmware subchunk received.\n");
                }

                if(sub_chunk_no==1){
                    if(subchunk_offset!=0){
                        subchunk_offset=0;
                        LOG_WARN("Something went wrong during OTA uart transfer between the gateway and the sink. Probably the last sub_chuck of the previous chunk has been lost. chunk_no: %u, sub_chunk_no: %u\n",chunk_no, sub_chunk_no);
                        continue; //discard the data
                    }
                }else{
                    if(sub_chunk_no!=((last_received_subchunk+1)&0xFF)){
                        last_received_subchunk=0;
                        LOG_WARN("Something went wrong during OTA uart transfer between the gateway and the sink. sub_chunk_no is not the expected one. chunk_no: %u, sub_chunk_no: %u\n",chunk_no, sub_chunk_no);
                        continue; //discard the data
                    }
                }

                if(sub_chunk_data_size+subchunk_offset <= OTA_CHUNK_SIZE){
                    chunk.chunk_no=chunk_no;

                    memcpy(&chunk.p_data[subchunk_offset], &byte_array[idx], sub_chunk_data_size); //-2 is for the subchunk_no and chunk_no
                    memcpy(chunk.p_destination,&outgoing_network_message_ipaddr,sizeof(uip_ipaddr_t));
                    
                    //LOG_INFO("Received OTA subchunk. Stored locally at offset: %u.\n",subchunk_offset);
                    subchunk_offset += sub_chunk_data_size;
                    last_received_subchunk=sub_chunk_no;

                    if(subchunk_offset==OTA_CHUNK_SIZE || is_last){ //the GW have to break the fimware into chunks of 1024 bytes, each chunk should be further broken into subchunks to be sent over the uart (the SERIAL_LINE_CONF_BUFSIZE is not enough to contain a full chunk). Once the full chunk of 1024bytes is sent the GW must wait the ack from the node before sending another chunk.
                        
                        chunk.data_len=subchunk_offset;                
                        chunk.chunkID++;
                        chunk.is_first=is_first;
                        chunk.is_last=is_last;
                        process_post(&sink_receiver_process, event_code_ready, &chunk);
                        subchunk_offset=0;
                        is_first=0;
                        is_last=0;
                        last_received_subchunk=0;
                    }

	                leds_toggle(LEDS_RED);
                }else{
                    LOG_ERR("A buffer overflow was happening in the OTA chunk buffer.\n");
                }
            }else if(pkttype==ota_reboot_node){
                send_to_node(&byte_array[NET_MESS_MSGADDR_LEN+NET_MESS_MSGTYPE_LEN+NET_MESS_MSGLEN_LEN], payload_size, NULL, 0, pkttype, &outgoing_network_message_ipaddr);
            }else{
                LOG_WARN("Unknown packet type (0x%04x) received from the uart\n",pkttype);
            }
        }
      }else{
        LOG_WARN("Unvalid packet received on uart. serial_line_len: %u , packet: 0x",(unsigned int)serial_line_len);
        for(uint8_t iii = 0; iii < serial_line_len; iii++){
            LOG_WARN_("%02x",input[iii]);
        }
        LOG_WARN_("\n");
      }
    } 

    //if (etimer_expired(&periodicSinkOutput)) {
    //  etimer_reset(&periodicSinkOutput);
    //  LOG_INFO("---------------------NodeCount: %d lost = %d\n",uip_sr_num_nodes(), lost);
    //}
  }
  PROCESS_END();
}

