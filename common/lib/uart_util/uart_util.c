/*
*Copyright 2017 Fondazione Bruno Kessler
*
*Licensed under the Apache License, Version 2.0 (the "License");
*you may not use this file except in compliance with the License.
*You may obtain a copy of the License at
*
*    http://www.apache.org/licenses/LICENSE-2.0
*
*Unless required by applicable law or agreed to in writing, software
*distributed under the License is distributed on an "AS IS" BASIS,
*WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*See the License for the specific language governing permissions and
*limitations under the License.
*/

#include "uart_util.h"
#include "constraints.h"

#ifdef CONTIKI
#include "dev/cc26xx-uart.h"
#include "dev/serial-line.h"
#include "dev/leds.h"
#include "sys/ctimer.h"
#include "ti-lib.h"
#define UART_ACK_TIMEOUT 						CLOCK_SECOND/2			//for small reasonable values (from 1 to 10) it doesn't work, the timeout handler triggered on the first transfer. May it be that contiki takes a long time to reschedule the process??
#else
#define UART_ACK_TIMEOUT						APP_TIMER_TICKS(500)
#endif

//TODO: Maybe it is better to use the serial library and implement the same using the DMA

typedef enum{
	waiting_start_seq,
	receiving_len_field,
	receiving_packet,
	waiting_end_seq,
	error,
} uart_rx_status_t;

uint8_t is_an_ack(uart_pkt_t* p_packet);
uint8_t uart_util_ack_check(uart_pkt_t* p_packet);
static void stop_wait_for_ack(void);
#ifdef CONTIKI
void enable_uart_flow_control(void);
#endif
#ifndef CONTIKI
static void sleep_handler(void);
void serial_evt_handle(struct nrf_serial_s const * p_serial, nrf_serial_event_t event);

NRF_SERIAL_DRV_UART_CONFIG_DEF(m_uart0_drv_config,
								UART_RX_PIN_NUMBER, UART_TX_PIN_NUMBER,
								UART_RTS_PIN_NUMBER, UART_CTS_PIN_NUMBER,
								UART_FLOW_CONTROL, UART_PARITY_ENABLED,
								UART_BAUDRATE,
								UART_DEFAULT_CONFIG_IRQ_PRIORITY);


NRF_SERIAL_QUEUES_DEF(serial_queues, SERIAL_FIFO_TX_SIZE, SERIAL_FIFO_RX_SIZE);

NRF_SERIAL_BUFFERS_DEF(serial_buffs, SERIAL_BUFF_TX_SIZE, SERIAL_BUFF_RX_SIZE);

NRF_SERIAL_UART_DEF(serial_uart, 0);

NRF_SERIAL_CONFIG_DEF(serial_config, NRF_SERIAL_MODE_IRQ, &serial_queues, &serial_buffs, serial_evt_handle, sleep_handler);

APP_TIMER_DEF(m_ack_timer_id);

static void sleep_handler(void)
{
	(void) sd_app_evt_wait();
}
#endif

ack_wait_t m_ack_wait = {
		false,
		0,
		0,
		uart_evt_ready,
		0
};

#ifdef CONTIKI
static struct ctimer m_ack_timer;
#endif

uint8_t to_byte(uint8_t hex_data){
	if( hex_data <= '9'){ //0x'0' to 0x'9'
		return hex_data - '0';
	}else{  //0x'A' to 0x'F'
		return hex_data - 'A' + 10;
	}
}

uint8_t to_hex(uint8_t i){
	return (i <= 9 ? '0' + i : 'A' - 10 + i);
}

uint32_t hex_array_to_uint8(uint8_t *p_hex_data, uint16_t len, uint8_t* p_byte_data){
	if(p_hex_data == NULL || len == 0 || p_byte_data == 0){
		return 0;
	}
	uint32_t i;
	for(i = 0; i < len/2; i++){
		p_byte_data[i] = ((0x0F & to_byte(p_hex_data[2*i])) << 4) + ( 0x0F & to_byte(p_hex_data[2*i + 1]));
	}
	return i;
}

uint32_t uint8_to_hex_array(uint8_t *p_byte_data, uint32_t len, uint8_t* p_hex_data, uint8_t max_len) {
	if(p_byte_data == NULL || len == 0 || max_len == 0){
		return 0;
	}
	uint8_t str_i = 0;
	for(int i = 0; i < len && str_i+1 <= max_len; i++){
		p_hex_data[str_i++] = to_hex( ((p_byte_data[i]>>4) & 0x0F) );
		p_hex_data[str_i++] = to_hex( ((p_byte_data[i]) & 0x0F) );
	}
	return str_i;
}

uint8_t is_start_sequence(uint8_t *data){
	return data[0] == UART_PKT_START_SYMBOL; //it may search on multiple array position, for now keep it simple and process one char at the time
}

uint8_t is_end_sequence(uint8_t *data){
#ifndef CONTIKI
	return data[0] == UART_PKT_END_SYMBOL; //it may search on multiple array position, for now keep it simple and process one char at the time
#else
	return data[0] == '\0';//it may search on multiple array position, for now keep it simple and process one char at the time
#endif
}

#ifdef CONTIKI
uint32_t contiki_serial_read(uint8_t *source, uint8_t *dest, uint16_t max_len, size_t *written_size){
	if(max_len >= 1){
		written_size[0] = 1;
		dest[0] = source[0];
		return NRF_SUCCESS;
	}else{
		written_size[0] = 0;
		return NRF_ERROR_DATA_SIZE;
	}
}
#endif

//Function for retrieving UART characters. It reacts to APP_UART_DATA_READY event
//once it find the END_OF_TX char it calls uart_util_rx_handler(...).
#ifndef CONTIKI
static void process_uart_rx_data() {
#else
void process_uart_rx_data(uint8_t *serial_data){
#endif
	static uint8_t uart_frame[UART_FRAME_MAX_LEN_BYTE];
	static uint8_t idx = 0;
	static uint16_t expected_lenght = 0;
	static uart_rx_status_t status = waiting_start_seq;
	uart_rx_status_t new_status = waiting_start_seq;

	uint8_t temp_array[HEX_FRAME_LEN_LEN_SYMB];
	size_t written_size = 0;
	uint32_t ret = NRF_SUCCESS;

#ifdef CONTIKI
	uint8_t done = 0;
	while ( !done ) {
#endif

	switch(status){
	case error:	//every time an error occur just restart form waiting_start_seq state
	case waiting_start_seq:
		idx = 0;	//force the packet to be reset when in this state
		expected_lenght = 0;
#ifndef CONTIKI
		ret = nrf_serial_read(&serial_uart, &uart_frame[idx], UART_FRAME_MAX_LEN_BYTE - idx, &written_size, 0);
#else
		ret = contiki_serial_read(&serial_data[idx], &uart_frame[idx], UART_FRAME_MAX_LEN_BYTE - idx, &written_size);
#endif
		if(written_size == 0){
			new_status = status;
			break;
		}
		if(ret != NRF_SUCCESS && ret != NRF_ERROR_TIMEOUT){
			new_status = error; //this shoud go to error...but maybe it is better to stay here
		}
		if( is_start_sequence(&uart_frame[idx]) ){
			idx += written_size;
			new_status = receiving_len_field;
		}else{
			new_status = error;
		}
		break;

	case receiving_len_field:
#ifndef CONTIKI
		ret = nrf_serial_read(&serial_uart, &uart_frame[idx], UART_FRAME_MAX_LEN_BYTE - idx, &written_size, 0);
#else
		ret = contiki_serial_read(&serial_data[idx], &uart_frame[idx], UART_FRAME_MAX_LEN_BYTE - idx, &written_size);
#endif
		if(written_size == 0){
			new_status = status;
			break;
		}
		idx += written_size;

		if( ret != NRF_SUCCESS && ret != NRF_ERROR_TIMEOUT ) {
			new_status = error;
			break;
		}

		if( idx >= UART_FRAME_START_SEQ_LEN_BYTE + HEX_FRAME_LEN_LEN_BYTE ){	//we have just received the length field
			ret = hex_array_to_uint8(&uart_frame[UART_FRAME_START_SEQ_LEN_BYTE], HEX_FRAME_LEN_LEN_BYTE, temp_array);
			expected_lenght = 2 * ( (temp_array[0] << 8) + temp_array[1] );
			if(expected_lenght == 0 || expected_lenght > UART_PKT_HEX_FRAME_MAX_LEN_BYTE){
				new_status = error;
			}else{
				new_status = receiving_packet;
			}
		}else{
			new_status = status; //remain here till the whole LEN field is received
		}
		break;

	case receiving_packet:
#ifndef CONTIKI
		ret = nrf_serial_read(&serial_uart, &uart_frame[idx], UART_FRAME_MAX_LEN_BYTE - idx, &written_size, 0);
#else
		ret = contiki_serial_read(&serial_data[idx], &uart_frame[idx], UART_FRAME_MAX_LEN_BYTE - idx, &written_size);
#endif
		if(written_size == 0){
			new_status = status;
			break;
		}
		idx += written_size;

		if (ret != NRF_SUCCESS && ret != NRF_ERROR_TIMEOUT) {
			new_status = error;
			break;
		}

		if (idx >= UART_FRAME_START_SEQ_LEN_BYTE + HEX_FRAME_LEN_LEN_BYTE + expected_lenght) {	//we have just received the length field
			new_status = waiting_end_seq;
		}else{
			new_status = status; //remain here till the whole LEN field is received
		}
		break;

	case waiting_end_seq:
#ifndef CONTIKI
		ret = nrf_serial_read(&serial_uart, &uart_frame[idx], UART_FRAME_MAX_LEN_BYTE - idx, &written_size, 0);
#else
		ret = contiki_serial_read(&serial_data[idx], &uart_frame[idx], UART_FRAME_MAX_LEN_BYTE - idx, &written_size); //todo: in place of idx use the calculated position for the end of tx character
#endif
		if(written_size == 0){
			new_status = status;
			break;
		}
		if(ret != NRF_SUCCESS && ret != NRF_ERROR_TIMEOUT){
			new_status = error; //this shoud go to error...but maybe it is better to stay here
		}
		idx += written_size;
		if( is_end_sequence( &uart_frame[ UART_FRAME_START_SEQ_LEN_BYTE + HEX_FRAME_LEN_LEN_BYTE + expected_lenght ]) ){
			idx++;

			uint8_t *uart_packet = &uart_frame[UART_FRAME_START_SEQ_LEN_BYTE + HEX_FRAME_LEN_LEN_BYTE]; //THIS IS FOR REUSE THE SPACE!, IF IT LEADS TO PROBLEM ALLOCATE A NEW ARRAY
			uint32_t byte_len = hex_array_to_uint8(&uart_frame[UART_FRAME_START_SEQ_LEN_BYTE + HEX_FRAME_LEN_LEN_BYTE], expected_lenght , uart_packet);
			uart_pkt_t p_packet;
			p_packet.type = (uart_pkt_type_t) ((uart_packet[0] << 8) + uart_packet[1]); //TODO: WHAT'S HAPPEN IF THE CAST CANNOT BE DONE?????
			if(expected_lenght == 0){
				p_packet.payload.p_data = NULL;
			}else{
				p_packet.payload.p_data = &uart_packet[UART_PKT_TYPE_LEN_SYMB];
			}
			p_packet.payload.data_len = byte_len - UART_PKT_TYPE_LEN_SYMB;

			uart_util_ack_check(&p_packet);
			uart_util_rx_handler(&p_packet);

#ifdef CONTIKI
			done = 1;
#endif
			new_status = waiting_start_seq;
		}else{
			new_status = error;
		}
		break;

	default:
		break;
	}
	status = new_status; 	//update the status for the next round
#ifdef CONTIKI
	}
#endif
	return;
}

//returns true if the packet has to be passed to the application for processing (if it is not an ack and we are not waiting acks). These packets needs ack!
//false if the packet was an ack
uint8_t uart_util_ack_check(uart_pkt_t* p_packet) {
	if (is_an_ack(p_packet)) {
		if (m_ack_wait.ack_waiting) {
			stop_wait_for_ack();
		}else{
			//we received and ack while we were not waiting it, then discard it!
		}
		return 0; //in any case ack pacekts are not passed to the application
	}else{
		if (m_ack_wait.ack_waiting) { //if we receive an generic packet while we were waiting an ack, stop waiting and signal the error
			ack_wait_t old_wait = m_ack_wait; //stop_wait_for_ack() resets the m_ack_wait variable. So we store a temporary version to pass it to uart_util_ack_error(...)
			stop_wait_for_ack();
			uart_util_ack_error(&old_wait);
			return 0;
		}else{ //if we receive a generic packet while we were not waiting anything just return true to pass the packet to the application
			return 1;
		}
	}
}


#ifndef CONTIKI
//Function for handling uart events (this function is called by the system)
void serial_evt_handle(struct nrf_serial_s const * p_serial, nrf_serial_event_t event)
{
	switch(event){
	case NRF_SERIAL_EVENT_TX_DONE:
		break;
	case NRF_SERIAL_EVENT_RX_DATA:
		process_uart_rx_data();
		break;
	case NRF_SERIAL_EVENT_DRV_ERR: //should be triggered only if the FIFO is not enabled
		break;
	case NRF_SERIAL_EVENT_FIFO_ERR:
		break;
	default:
		break;
	}
}
#endif


uint8_t is_an_ack(uart_pkt_t* p_packet){
	if( p_packet->type == uart_app_level_ack ){ //todo check also the state of the ack
//		uart_pkt_type_t ack_for_type = (p_packet->payload[2] << 8) + p_packet->payload[3];
		return 1;
	}else{
		return 0;
	}
}

#ifdef CONTIKI
void ack_timeout_handler(void *ptr){
	ctimer_stop(&m_ack_timer);
#else
void ack_timeout_handler(){
#endif
	if (m_ack_wait.ack_waiting) {
		ack_wait_t old_wait = m_ack_wait; //stop_wait_for_ack() resets the m_ack_wait variable. So we store a temporary version to pass it to uart_util_ack_error(...)
		stop_wait_for_ack();
		uart_util_ack_error(&old_wait);
	}else{ //if ack_timeout_handler() is triggered but we are not waiting an ack, reset variables but not call packet_error_handler
		stop_wait_for_ack();
	}
}


static void stop_wait_for_ack(void){
	// Start application timers.
	m_ack_wait.ack_waiting = false;
	m_ack_wait.ack_wait_timeout_ticks = 0;
	m_ack_wait.ack_wait_start_ticks = 0;
	m_ack_wait.waiting_ack_for_pkt_type = uart_evt_ready;
#ifndef CONTIKI
	ret_code_t err_code = app_timer_stop(m_ack_timer_id);
	APP_ERROR_CHECK(err_code);
#else
	ctimer_stop(&m_ack_timer);
#endif
}

static void start_wait_for_ack(uint32_t timeout, uart_pkt_t *p_paket){ //timeout is in APP_TIMER ticks!

#ifndef CONTIKI
    ret_code_t err_code;
    // Start application timers.
    m_ack_wait.ack_waiting = true;
    m_ack_wait.ack_wait_timeout_ticks = timeout;
    m_ack_wait.ack_wait_start_ticks = app_timer_cnt_get();
    m_ack_wait.waiting_ack_for_pkt_type = p_paket->type;
    err_code = app_timer_start(m_ack_timer_id, timeout, NULL);
    APP_ERROR_CHECK(err_code);
#else
    m_ack_wait.ack_waiting = true;
    m_ack_wait.ack_wait_timeout_ticks = timeout;
    m_ack_wait.ack_wait_start_ticks = clock_time();
    m_ack_wait.waiting_ack_for_pkt_type = p_paket->type;

    ctimer_set(&m_ack_timer, timeout, ack_timeout_handler, NULL); //the smallest timeout is 1/CLOCK_SECOND. Here it is 1/128 s, i.e. 8ms, which is quite fine for a timeout
#endif
}

#ifdef CONTIKI
unsigned int uart1_send_bytes(const unsigned char *s, unsigned int len)
{
  unsigned int i = 0;

  while(s && *s != 0) {
    if(i >= len) {
      break;
    }
    cc26xx_uart_write_byte(*s++);	//NB: this can block the execution
    i++;
  }

  if(i == len){
	 return NRF_SUCCESS;
  }else{
	 return NRF_ERROR_INTERNAL;
  }

}
#endif

uint32_t uart_util_send_pkt(uart_pkt_t* uart_pkt_t){

	if(uart_pkt_t == NULL){
		return NRF_ERROR_NULL;
	}

	if( uart_pkt_t->payload.data_len > UART_PKT_PAYLOAD_MAX_LEN_SYMB ){
		return NRF_ERROR_INVALID_LENGTH;
	}

	if( uart_pkt_t->payload.p_data == NULL && uart_pkt_t->payload.data_len > 0){
		return NRF_ERROR_NULL;
	}

	if(m_ack_wait.ack_waiting){
		return NRF_ERROR_INVALID_STATE;
	}

	uint16_t idx = 0;
	uint32_t len = 0;
	//calculate the lenght in byte for allocating memory
	uint8_t uart_frame_len = UART_FRAME_START_SEQ_LEN_BYTE + 2*(uart_pkt_t->payload.data_len) + HEX_FRAME_LEN_LEN_BYTE + UART_PKT_TYPE_LEN_BYTE + UART_FRAME_END_SEQ_LEN_BYTE;
	uint8_t uart_frame[uart_frame_len];
	//memset(uart_frame, 0x00, uart_frame_len*sizeof(uint8_t));

	//write start of packet symbol
	uart_frame[idx++] = UART_PKT_START_SYMBOL;
	//calculate content of packet length field (it will  contain the length in hex formatted characters which is half of the length in byte)
	uint16_t hex_len_field = UART_PKT_TYPE_LEN_SYMB + uart_pkt_t->payload.data_len;
	uint8_t hex_len_field_array[UART_PKT_TYPE_LEN_SYMB] = {0xFF & (hex_len_field >> 8), 0xFF & (hex_len_field)};
	//write content of packet length field to the packet
	len = uint8_to_hex_array(hex_len_field_array,HEX_FRAME_LEN_LEN_BYTE, &uart_frame[idx], HEX_FRAME_LEN_LEN_BYTE);
	idx += len;
	//write content of packet type field to the packet
	uint8_t type_field_array[UART_PKT_TYPE_LEN_BYTE] = {0xFF & ((uart_pkt_t->type) >> 8), 0xFF & (uart_pkt_t->type)};
	len = uint8_to_hex_array(type_field_array,UART_PKT_TYPE_LEN_BYTE, &uart_frame[idx], UART_PKT_TYPE_LEN_BYTE);
	idx += len;
	//write content of payload field to the packet
	len = uint8_to_hex_array(uart_pkt_t->payload.p_data,2*(uart_pkt_t->payload.data_len), &uart_frame[idx], 2*(uart_pkt_t->payload.data_len));
	idx += len;
	//write end of packet symbol
	uart_frame[idx++] = UART_PKT_END_SYMBOL;

#ifndef CONTIKI
	uint32_t ret = nrf_serial_write(&serial_uart,
							uart_frame,
							idx,
                            NULL,
                            0);
#else
	uint32_t ret = uart1_send_bytes(uart_frame, idx);
#endif

	if(ret == NRF_SUCCESS){
		if(uart_pkt_t->type != uart_app_level_ack){ //do not send acknowledge to ack packets
			start_wait_for_ack(UART_ACK_TIMEOUT, uart_pkt_t);
		}
	}

	return ret;
}


void uart_util_send_ack(uart_pkt_t* p_packet, uint8_t error_code){
	uart_pkt_t ack_pkt;
	ack_pkt.type = uart_app_level_ack;
	uint8_t ack_data[] = {
			  	  	  	  error_code,
						  0xFF & ((p_packet->payload.data_len) >> 8),
						  0xFF & (p_packet->payload.data_len),
						  0xFF & ((p_packet->type) >> 8),
			  	  	      0xFF & (p_packet->type),
	};
	ack_pkt.payload.p_data = ack_data;
	ack_pkt.payload.data_len = 5; //TODO: for now it is hardcoded, use defines or enum

	uart_util_send_pkt(&ack_pkt);

	uart_util_ack_tx_done();
}

#ifdef CONTIKI
void enable_uart_flow_control(void){

	/* disable the UART */
    ti_lib_uart_disable(UART0_BASE);

    /* Disable all UART interrupts and clear all flags */
    /* Acknowledge UART interrupts */
    ti_lib_int_disable(INT_UART0_COMB);

    /* Disable all UART module interrupts */
    ti_lib_uart_int_disable(UART0_BASE, UART_INT_OE | UART_INT_BE | UART_INT_PE | UART_INT_FE | UART_INT_RT | UART_INT_TX | UART_INT_RX | UART_INT_CTS);

    /* Clear all UART interrupts */
    ti_lib_uart_int_clear(UART0_BASE, UART_INT_OE | UART_INT_BE | UART_INT_PE | UART_INT_FE | UART_INT_RT | UART_INT_TX | UART_INT_RX | UART_INT_CTS);

    /* Configure hardware flow control if the CTS and RTS are assigned */
    ti_lib_uart_hw_flow_control_en(UART0_BASE);

      /* Enable UART interrupts */
    /* Configure which interrupts to generate: FIFO level or after RX timeout */
    ti_lib_uart_int_enable(UART0_BASE, UART_INT_RX | UART_INT_RT);

    /* Acknowledge UART interrupts */
    ti_lib_int_enable(INT_UART0_COMB);

    /* enable the UART */
    ti_lib_uart_enable(UART0_BASE);
}
#endif
//initialize uart based on defines in uart_util.h
void uart_util_initialize(void){
#ifndef CONTIKI
    ret_code_t ret = nrf_serial_init(&serial_uart, &m_uart0_drv_config, &serial_config);
    APP_ERROR_CHECK(ret);

    ret = app_timer_create(&m_ack_timer_id,
    							APP_TIMER_MODE_SINGLE_SHOT,
                                ack_timeout_handler);

    APP_ERROR_CHECK(ret);
#else
#if BOARD_IOID_UART_CTS != IOID_UNUSED && BOARD_IOID_UART_RTS != IOID_UNUSED
    enable_uart_flow_control();
#endif
#endif
}

void uart_util_flush(void){
#ifndef CONTIKI
    ret_code_t ret = nrf_serial_flush(&serial_uart, 0);
    APP_ERROR_CHECK(ret);
#endif
}

