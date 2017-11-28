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

#include "contiki.h"
#include "sys/etimer.h"
#include "dev/leds.h"
#include "dev/cc26xx-uart.h"
#include "dev/serial-line.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "uart_util.h"
#include "constraints.h"
#include "sys/ctimer.h"
#include "vela_uart.h"

PROCESS(cc2650_uart_process, "cc2650 uart process");
// ALM -- removed next line
//AUTOSTART_PROCESSES(&cc2650_uart_process);

typedef enum{
	wait,
	initializing1,
	initializing2,
	initializing3,
	initializing4,
	run,
	resetting,
} app_state_t;

typedef enum{
	start_of_data,
	more_data,
	end_of_data
} report_type_t;

#define MAX_MESH_PAYLOAD_SIZE	MAX_NUMBER_OF_BT_BEACONS*SINGLE_NODE_REPORT_SIZE

#define PING_PACKET_SIZE		50
#define BT_REPORT_BUFFER_SIZE 	MAX_MESH_PAYLOAD_SIZE
#define REPORT_TIMEOUT_MS		5000
#define PING_TIMEOUT 			CLOCK_SECOND*5

/** Converts a macro argument into a character constant.
 */
#define STRINGIFY(val)  #val

static struct ctimer m_ping_timer;

app_state_t m_app_state = wait;
uint8_t no_of_attempts = 0;
uint8_t bt_report_buffer[BT_REPORT_BUFFER_SIZE];
data_t complete_report_data = {bt_report_buffer, 0};

uint32_t report_ready(data_t *p_data);
uint32_t report_rx_handler(uart_pkt_t* p_packet, report_type_t m_report_type );

void ping_timeout_handler(void *ptr);

void fsm_state_update(void);
void fsm_state_process(void);
void fsm_stop(void);
void fsm_start(void);
void fsm_reset(void);
void reset_nodric(void);

void send_ping(void);
void send_set_bt_scan_on(void);
void send_set_bt_scan_params(void);
void send_ready(void);
void send_reset(void);
void request_periodic_report_to_nordic(uint32_t report_timeout_ms);

void uart_util_rx_handler(uart_pkt_t* p_packet);
void uart_util_ack_tx_done(void);
extern void uart_util_ack_error(ack_wait_t* ack_wait_data);

//ALM -- called to start this as a contiki THREAD
void vela_uart_init() {
  // start the uart process
  process_start(&cc2650_uart_process, "cc2650 uart process");
  return;

}


uint32_t report_ready(data_t *p_data){
	//PROCESS REPORT: return APP_ACK_SUCCESS as soon as possible
	process_post(&vela_sender_process, event_data_ready, p_data);
	return APP_ACK_SUCCESS;
}

uint32_t report_rx_handler(uart_pkt_t* p_packet, report_type_t m_report_type ){
	static uint16_t buff_free_from = 0;
	uint32_t ret;

	switch(m_report_type){
	case start_of_data:
		buff_free_from = 0;
		if((buff_free_from + p_packet->payload.data_len) < BT_REPORT_BUFFER_SIZE){
			memcpy(&bt_report_buffer[buff_free_from], p_packet->payload.p_data, p_packet->payload.data_len);
			buff_free_from+=p_packet->payload.data_len;
			ret = APP_ACK_SUCCESS;
		}else{
			ret = APP_ERROR_NO_MEM;
		}
		break;
	case more_data:
		if((buff_free_from + p_packet->payload.data_len) < BT_REPORT_BUFFER_SIZE){
			memcpy(&bt_report_buffer[buff_free_from], p_packet->payload.p_data, p_packet->payload.data_len);
			buff_free_from+=p_packet->payload.data_len;
			ret = APP_ACK_SUCCESS;
		}else{
			ret = APP_ERROR_NO_MEM;
		}
		break;
	case end_of_data:
		if((buff_free_from + p_packet->payload.data_len) < BT_REPORT_BUFFER_SIZE){
			memcpy(&bt_report_buffer[buff_free_from], p_packet->payload.p_data, p_packet->payload.data_len);

			complete_report_data.data_len = buff_free_from + p_packet->payload.data_len;
			complete_report_data.p_data = bt_report_buffer;
			ret = report_ready(&complete_report_data);
		}else{
			ret = APP_ERROR_NO_MEM;
		}
		buff_free_from = 0;
		break;
	default:
		ret = APP_ERROR_COMMAND_NOT_VALID;
		break;
	}

	return ret;
}

void ping_timeout_handler(void *ptr){
	ctimer_reset(&m_ping_timer);

	send_ping();
}

void fsm_state_update(void) {

	app_state_t new_app_state = m_app_state;
	switch (m_app_state) {
	case wait:
		new_app_state = wait;
		break;
	case initializing1:
		new_app_state = initializing2;
		break;
	case initializing2:
		new_app_state = initializing3;
		break;
	case initializing3:
		new_app_state = initializing4;
		break;
	case initializing4:
		new_app_state = run;
		break;
	case run:
		new_app_state = run;
		break;
	case resetting:
		new_app_state = initializing1;
		break;
	default:
		break;
	}

	m_app_state = new_app_state;
}

void fsm_state_process(void) {
	leds_off(LEDS_RED);
	switch (m_app_state) {
	case wait:
		ctimer_set(&m_ping_timer, PING_TIMEOUT, ping_timeout_handler, NULL);
		break;
	case initializing1:
		ctimer_stop(&m_ping_timer); //stop pinging
		send_ready();
		break;
	case initializing2:
		send_set_bt_scan_params();
		break;
	case initializing3:
		send_set_bt_scan_on();
		break;
	case initializing4:
		request_periodic_report_to_nordic(REPORT_TIMEOUT_MS);
		break;
	case run:
		leds_on(LEDS_GREEN);
		//nothing to do
		break;
	case resetting:
		send_reset();
		break;
	default:
		break;
	}
}

void fsm_stop(void){
	m_app_state = wait;
	fsm_state_process();
}

void fsm_start(void) {
	fsm_reset();
	fsm_state_process();
}

void fsm_reset(void){
	no_of_attempts = 1;
	m_app_state = initializing1;
}

#define RESET_PIN_IOID			IOID_1
void reset_nodric(void){
	GPIO_clearDio(RESET_PIN_IOID);
	clock_wait(CLOCK_SECOND/10);
	GPIO_setDio(RESET_PIN_IOID);
}


//example of use of uart_util_send. This function is called with a timer and send a packet to the UART
void send_ping(void) {
/*  CREATE DUMMY UINT8 ARRAY   */
#if PING_PACKET_SIZE != 0
	uint8_t payload[PING_PACKET_SIZE];
	for (uint16_t i = 0; i < PING_PACKET_SIZE; i++) {
		payload[i] = PING_PACKET_SIZE - i;
	}
	/*  CREATE A PACKET VARIABLE */
	uart_pkt_t packet;
	packet.payload.p_data = payload;
	packet.payload.data_len = PING_PACKET_SIZE;
	packet.type = uart_ping;
#else
	uart_pkt_t packet;
	packet.payload.p_data = NULL;
	packet.payload.data_len = PING_PACKET_SIZE;
	packet.type = uart_ping;
#endif
	/*  SEND THE PACKET  */
	uart_util_send_pkt(&packet);
}

static void send_pong(uart_pkt_t* p_packet) {
	if(p_packet->payload.data_len){
		uint8_t pong_payload[p_packet->payload.data_len];
		memcpy(p_packet->payload.p_data, pong_payload, p_packet->payload.data_len);

		uart_pkt_t pong_packet;
		pong_packet.payload.p_data = pong_payload;
		pong_packet.payload.data_len = p_packet->payload.data_len;
		pong_packet.type = uart_pong;

		uart_util_send_pkt(&pong_packet);
	}else{
		uart_pkt_t pong_packet;
		pong_packet.payload.p_data = NULL;
		pong_packet.payload.data_len = p_packet->payload.data_len;
		pong_packet.type = uart_pong;

		uart_util_send_pkt(&pong_packet);
	}
}

void send_set_bt_scan_on(void) {
	uart_pkt_t pong_packet;
	uint8_t payload[1];
	pong_packet.payload.p_data = payload;
	pong_packet.payload.data_len = 1;
	pong_packet.type = uart_set_bt_scan_state;

	payload[0] = 1;

	uart_util_send_pkt(&pong_packet);
}

void send_set_bt_scan_params(void) {
	uart_pkt_t pong_packet;
	uint8_t payload[7];
	pong_packet.payload.p_data = payload;
	pong_packet.payload.data_len = 7;
	pong_packet.type = uart_set_bt_scan_params;

	uint8_t active_scan = 1;
	uint16_t scan_interval = 8;
	uint16_t scan_window = 8;
	uint16_t timeout = 0;

	payload[0] = active_scan; //active_scan
	payload[1] = scan_interval >> 8;
	payload[2] = scan_interval;
	payload[3] = scan_window >> 8;
	payload[4] = scan_window;
	payload[5] = timeout >> 8;
	payload[6] = timeout;

	uart_util_send_pkt(&pong_packet);
}

void send_ready(void) {

	char version_string[] = STRINGIFY(VERSION_STRING);

	uart_pkt_t pong_packet;
	pong_packet.payload.p_data = (uint8_t*)version_string;
	pong_packet.payload.data_len = strlen(version_string);
	pong_packet.type = uart_evt_ready;

	uart_util_send_pkt(&pong_packet);
}

void send_reset(void) {
	uart_pkt_t packet;
	packet.payload.p_data = NULL;
	packet.payload.data_len = 0;
	packet.type = uart_req_reset;

	uart_util_send_pkt(&packet);
}

void request_periodic_report_to_nordic(uint32_t report_timeout_ms){
	uint8_t payload[4];
	payload[0] = report_timeout_ms >> 24;
	payload[1] = report_timeout_ms >> 16;
	payload[2] = report_timeout_ms >> 8;
	payload[3] = report_timeout_ms;


	uart_pkt_t packet;
	packet.payload.p_data = payload;
	packet.payload.data_len = 4;
	packet.type = uart_req_bt_neigh_rep;

	uart_util_send_pkt(&packet);
}

#define MAX_ATTEMPTS 3
void uart_util_ack_error(ack_wait_t* ack_wait_data) {
	leds_on(LEDS_RED);
	if(m_app_state == wait){
		return;
	}

	if(no_of_attempts < MAX_ATTEMPTS){
		no_of_attempts++;
		fsm_state_process(); //if there is an error try to resend
	}else{
		fsm_stop();
		reset_nodric(); //when the nordic will reboot it will generate the uart_ready evt. When the TI receives it, it restart the FSM
	}
}

void uart_util_ack_tx_done(void){
}

void uart_util_rx_handler(uart_pkt_t* p_packet) { //once it arrives here the ack is already sent

	uart_pkt_type_t type = p_packet->type;
	uint8_t *p_payload_data = p_packet->payload.p_data;
	uint32_t ack_value;

	switch (type) {
	case uart_app_level_ack:
		if (p_packet->payload.data_len == 5) {
			if (p_payload_data[0] == APP_ACK_SUCCESS) {//if the app ack is positive go on with fsm
				leds_off(LEDS_RED);
				no_of_attempts = 1;
				fsm_state_update();
				fsm_state_process();
			} else {	//otherwise call the ack error handler
				uart_util_ack_error(NULL); //attention, this function can be called also from uart_util when the timeout for ack expires
			}
		}
		return; //this avoids sending ack for ack messages
		break;
	case uart_evt_ready:
//		uart_util_send_ack(p_packet, APP_ACK_SUCCESS); //ack message for evt_ready can be avoided
		//here we shall wait the previous message to be sent, but if the uart buffers are enough long the two messages will fit together
		fsm_start();
		return;  //ready message may not send ack
		break;
	case uart_req_reset:
		ack_value = APP_ERROR_NOT_IMPLEMENTED;
//		sd_nvic_SystemReset();
		break;
	case uart_req_bt_state:
	case uart_req_bt_scan_state:
	case uart_req_bt_adv_state:
	case uart_req_bt_scan_params:
	case uart_req_bt_adv_params:
	case uart_req_bt_neigh_rep:
		ack_value = APP_ERROR_COMMAND_NOT_VALID;
		//never called on the Mesh side of the uart
		break;
	case uart_resp_bt_state:
		ack_value = APP_ERROR_NOT_IMPLEMENTED;
		//still not implemented
		break;
	case uart_resp_bt_scan_state:
		ack_value = APP_ERROR_NOT_IMPLEMENTED;
		//still not implemented
		break;
	case uart_resp_bt_adv_state:
		ack_value = APP_ERROR_NOT_IMPLEMENTED;
		//still not implemented
		break;
	case uart_resp_bt_scan_params:
		ack_value = APP_ERROR_NOT_IMPLEMENTED;
		//still not implemented
		break;
	case uart_resp_bt_adv_params:
		ack_value = APP_ERROR_NOT_IMPLEMENTED;
		//still not implemented
		break;
	case uart_resp_bt_neigh_rep_start:
		ack_value = report_rx_handler(p_packet, start_of_data);
		break;
	case uart_resp_bt_neigh_rep_more:
		ack_value = report_rx_handler(p_packet, more_data);
		break;
	case uart_resp_bt_neigh_rep_end:
		ack_value = report_rx_handler(p_packet, end_of_data);
		break;
	case uart_set_bt_state:
	case uart_set_bt_scan_state:
	case uart_set_bt_adv_state:
	case uart_set_bt_scan_params:
	case uart_set_bt_adv_params:
		ack_value = APP_ERROR_COMMAND_NOT_VALID;
		//never called on the Mesh side of the uart
		break;
	case uart_ping:
		ack_value = APP_ACK_SUCCESS;
		uart_util_send_ack(p_packet, ack_value); //send the ack before pong message
		//here we shall wait the previous message to be sent, but if the buffers are enough long the two messages will fit
		send_pong(p_packet);
		return;	//do not send the ack twice
		break;
	case uart_pong:
		ack_value = APP_ACK_SUCCESS;
		break;
	default:
		ack_value = APP_ERROR_NOT_FOUND;
		break;
	}

	uart_util_send_ack(p_packet, ack_value);
	return;
}

void initialize_reset_pin(void) {
	GPIO_setOutputEnableDio(RESET_PIN_IOID, GPIO_OUTPUT_ENABLE);
	GPIO_setDio(RESET_PIN_IOID);
}

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(cc2650_uart_process, ev, data) {
	PROCESS_BEGIN()
		;

		initialize_reset_pin();

		cc26xx_uart_set_input(serial_line_input_byte);

		uart_util_initialize();

		leds_off(LEDS_CONF_ALL);

		fsm_stop();

		reset_nodric();

		while (1) {

			PROCESS_WAIT_EVENT()
			;

			if (ev == serial_line_event_message) { //NB: *data contains a string, the '\n' character IS NOT included, the '\0' character IS included

				process_uart_rx_data((uint8_t*) data);
			}
		}

	PROCESS_END();
}
/*---------------------------------------------------------------------------*/
/**
 * @}
 * @}
 */
