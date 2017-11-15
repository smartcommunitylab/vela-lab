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

PROCESS(cc2650_uart_process, "cc2650 uart process");
AUTOSTART_PROCESSES(&cc2650_uart_process);

typedef enum{
	stop,
	initializing1,
	initializing2,
	initializing3,
	initializing4,
	run,
	resetting,
} app_state_t;

#define PING_PACKET_SIZE		0

uint8_t ready_transmitted = 0, request_transmitted = 0;
app_state_t m_app_state = stop;
uint8_t no_of_attempts = 0;

void fsm_state_update(void);
void fsm_state_process(void);
void fsm_start(void);
void fsm_reset(void);
void reset_nodric(void);

void send_ping(void);
void send_set_bt_scan_on(void);
void send_set_bt_scan_params(void);
void send_ready(void);
void send_reset(void);
void request_report_to_nordic(void);

void uart_util_rx_handler(uart_pkt_t* p_packet);
void uart_util_tx_done(void);
void uart_util_ack_error(void);

void fsm_state_update(void) {

	app_state_t new_app_state = m_app_state;
	switch (m_app_state) {
	case stop:
		new_app_state = stop;
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
	leds_off(LEDS_GREEN);
	switch (m_app_state) {
	case stop:
		break;
	case initializing1:
		send_ready();
		break;
	case initializing2:
		send_set_bt_scan_params();
		break;
	case initializing3:
		send_set_bt_scan_on();
		break;
	case initializing4:
		request_report_to_nordic();
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
	uart_pkt_t pong_packet;
	pong_packet.payload.p_data = NULL;
	pong_packet.payload.data_len = 0;
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

void request_report_to_nordic(void){
	uint8_t payload[4];
	uint32_t report_timeout_ms = 5000;
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
void uart_util_ack_error(void) {
	leds_on(LEDS_RED);
	if(no_of_attempts < MAX_ATTEMPTS){
		no_of_attempts++;
		fsm_state_process(); //if there is an error try to resend
	}else{
		reset_nodric(); //when the nordic will reboot it will generate the uart_ready evt. When the TI receives it, it restart the FSM
	}
}

void uart_util_tx_done(void){
	leds_off(LEDS_RED);
	no_of_attempts = 1;
	fsm_state_update();
	fsm_state_process();
}

void uart_util_rx_handler(uart_pkt_t* p_packet) { //once it arrives here the ack is already sent

	uart_pkt_type_t type = p_packet->type;

	switch (type) {
	case uart_ack:
		//nothing to do, event not reported by uart_util module
		break;
	case uart_evt_ready:	//if the nordic sends this it means that it has been rebooted, then restart the local finite state machine
		fsm_start();
		break;
	case uart_req_reset:
//		sd_nvic_SystemReset();
		break;
	case uart_req_bt_state:
		//still not implemented
		break;
	case uart_req_bt_scan_state:
		//still not implemented
		break;
	case uart_req_bt_adv_state:
		//still not implemented
		break;
	case uart_req_bt_scan_params:
		//still not implemented
		break;
	case uart_req_bt_adv_params:
		//still not implemented
		break;
	case uart_req_bt_neigh_rep:
		//not implemented on the TI side
		break;
	case uart_resp_bt_state:
		//still not implemented
		break;
	case uart_resp_bt_scan_state:
		//still not implemented
		break;
	case uart_resp_bt_adv_state:
		//still not implemented
		break;
	case uart_resp_bt_scan_params:
		//still not implemented
		break;
	case uart_resp_bt_adv_params:
		//still not implemented
		break;
	case uart_resp_bt_neigh_rep_start:
		leds_toggle(LEDS_YELLOW);
		//still not implemented
		break;
	case uart_resp_bt_neigh_rep_more:
		//still not implemented
		break;
	case uart_set_bt_state:
		//still not implemented
		break;
	case uart_set_bt_scan_state:
//		if(p_packet->payload.data_len == 1){
//			if(p_packet->payload.p_data[0]){ //enable scanner
//				scan_start();
//			}else{	//disable scanner
//				scan_stop();
//			}
//		}
		break;
	case uart_set_bt_adv_state:
		//still not implemented
		break;
	case uart_set_bt_scan_params:
		//still not implemented
		break;
	case uart_set_bt_adv_params:
		//still not implemented
		break;
	case uart_ping:
		send_pong(p_packet);
		break;
	case uart_pong:
		//nothing to do
		break;
	default:
		break;
	}
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
		leds_off(LEDS_CONF_ALL);

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
