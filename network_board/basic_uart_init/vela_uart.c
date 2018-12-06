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
#include "vela_sender.h"
#include "network_messages.h"

#define NORDIC_WATCHDOG_ENABLED						1

PROCESS(cc2650_uart_process, "cc2650 uart process");
// ALM -- removed next line
//AUTOSTART_PROCESSES(&cc2650_uart_process);

//typedef enum{
//	wait,
//	initializing1,
//	initializing2,
//	initializing3,
//	initializing4,
//	run,
//	resetting,
//} app_state_t;

typedef enum{
	start_of_data,
	more_data,
	end_of_data
} report_type_t;

#define MAX_MESH_PAYLOAD_SIZE	MAX_NUMBER_OF_BT_BEACONS*SINGLE_NODE_REPORT_SIZE

#define PING_PACKET_SIZE		5
#define BT_REPORT_BUFFER_SIZE 	MAX_MESH_PAYLOAD_SIZE
#define PING_TIMEOUT 			CLOCK_SECOND*5

#define DEFAULT_ACTIVE_SCAN         1     //boolean
#define DEFAULT_SCAN_INTERVAL       3520       /**< Scan interval between 0x0004 and 0x4000 in 0.625 ms units (2.5 ms to 10.24 s). */
#define DEFAULT_SCAN_WINDOW         1920     /**< Scan window between 0x0004 and 0x4000 in 0.625 ms units (2.5 ms to 10.24 s). */
#define DEFAULT_TIMEOUT             0
#define DEFAULT_REPORT_TIMEOUT_MS   101000

/** Converts a macro argument into a character constant.
 */
#define STRINGIFY(val)  #val

static struct ctimer m_ping_timer;

static uint8_t is_nordic_ready=false;
//static app_state_t m_app_state = wait;
static uint8_t no_of_attempts = 0;
static uint8_t bt_report_buffer[BT_REPORT_BUFFER_SIZE];
static data_t complete_report_data = {bt_report_buffer, 0};
#if NORDIC_WATCHDOG_ENABLED
static struct ctimer m_nordic_watchdog_timer;
static uint8_t nordic_watchdog_value = 0;
static uint32_t nordic_watchdog_timeout_ms = 0;
#endif
static uint8_t active_scan = DEFAULT_ACTIVE_SCAN;     //boolean
static uint16_t scan_interval = DEFAULT_SCAN_INTERVAL;       /**< Scan interval between 0x0004 and 0x4000 in 0.625 ms units (2.5 ms to 10.24 s). */
static uint16_t scan_window = DEFAULT_SCAN_WINDOW;     /**< Scan window between 0x0004 and 0x4000 in 0.625 ms units (2.5 ms to 10.24 s). */
static uint16_t timeout = DEFAULT_TIMEOUT;            /**< Scan timeout between 0x0001 and 0xFFFF in seconds, 0x0000 disables timeout. */
static uint32_t report_timeout_ms = DEFAULT_REPORT_TIMEOUT_MS;

typedef enum{
    stopped,
    running,
    executed,
} procedure_state_t;

typedef void (*m_f_ptr_t)(void);

typedef struct{
    uint8_t i;
    m_f_ptr_t * action;
    uint8_t size;
    procedure_state_t state;
    char name[];
}procedure_t;

static procedure_t* running_procedure=NULL;

uint32_t report_ready(data_t *p_data);
uint32_t report_rx_handler(uart_pkt_t* p_packet, report_type_t m_report_type );

void ping_timeout_handler(void *ptr);
#if NORDIC_WATCHDOG_ENABLED
void nordic_watchdog_handler(void *ptr);
#endif

void reset_nodric(void);

void send_ping(void);
void send_set_bt_scan_on(void);
void send_set_bt_scan_params(void);
void send_ready(void);
void send_reset(void);
void request_periodic_report_to_nordic(void);

void uart_util_rx_handler(uart_pkt_t* p_packet);
void uart_util_ack_tx_done(void);
extern void uart_util_ack_error(ack_wait_t* ack_wait_data);

//PROCEDURE DEFINITION. TODO: try to pack everything into a macro define.
void (*bluetooth_on_sequence[])(void) ={&send_set_bt_scan_params, &send_set_bt_scan_on, &request_periodic_report_to_nordic};
procedure_t bluetooth_on_procedure={0, bluetooth_on_sequence, sizeof(bluetooth_on_sequence)/sizeof(m_f_ptr_t) ,stopped,"bluetooth on"};

//ALM -- called to start this as a contiki THREAD
void vela_uart_init() {
  // start the uart process
  process_start(&cc2650_uart_process, "cc2650 uart process");
  return;

}


uint32_t report_ready(data_t *p_data){
	//PROCESS REPORT: return APP_ACK_SUCCESS as soon as possible
	process_post(&vela_sender_process, event_data_ready, p_data);
#if NORDIC_WATCHDOG_ENABLED
	nordic_watchdog_value = 0; //reset the nordic watchdog
#endif
	return APP_ACK_SUCCESS;
}
static uint16_t buff_free_from = 0;
static uint32_t ret = APP_ERROR_COMMAND_NOT_VALID;

uint32_t report_rx_handler(uart_pkt_t* p_packet, report_type_t m_report_type ){

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

#if NORDIC_WATCHDOG_ENABLED
void nordic_watchdog_handler(void *ptr){
	nordic_watchdog_value++;

	if(nordic_watchdog_value > 5){
		while(1){ //Stay here. The reset of the mcu will be triggered by the watchdog timer initialized into contiki-main.c
		}
	}else{
		if(nordic_watchdog_timeout_ms != 0){
			ctimer_set(&m_nordic_watchdog_timer, (nordic_watchdog_timeout_ms*CLOCK_SECOND)/1000, nordic_watchdog_handler, NULL);
		}
	}
}
#endif

uint8_t procedure_start(procedure_t *m_procedure){
    printf("Starting procedure \"%s\"\n",m_procedure->name);
    if(running_procedure->state != running){
        running_procedure = m_procedure;
        running_procedure->state=running;
        running_procedure->i=0;
        (*running_procedure->action[running_procedure->i])();
        return 1;
    }else{
        return 0;
    }
}

void procedure_step_on(){
    if(running_procedure != NULL && running_procedure->state != executed){
        (running_procedure->i)++;
        if(running_procedure->i < running_procedure->size){
            printf("Stepping on procedure \"%s\": action number %u\n",running_procedure->name,running_procedure->i);
            (*running_procedure->action[running_procedure->i])();
        }else{
            running_procedure->state=executed;
            printf("Procedure \"%s\" exectuted!\n",running_procedure->name);
        }
    }
}

void procedure_retry(){
    running_procedure->state=running;
    running_procedure->i=0;
    (*running_procedure->action[running_procedure->i])();
}

uint8_t procedure_is_running(){
    return running_procedure->state==running;
}

#define RESET_PIN_IOID			IOID_1
void reset_nodric(void){
	GPIO_clearDio(RESET_PIN_IOID);
	clock_wait(CLOCK_SECOND/10);
	GPIO_setDio(RESET_PIN_IOID);
	clock_wait(CLOCK_SECOND/10);
}


//example of use of uart_util_send. This function is called with a timer and send a packet to the UART
void send_ping(void) {
/*  CREATE DUMMY UINT8 ARRAY   */
#if PING_PACKET_SIZE != 0
	static uint8_t payload[PING_PACKET_SIZE];
	for (uint16_t i = 0; i < PING_PACKET_SIZE; i++) {
		payload[i] = PING_PACKET_SIZE - i;
	}
	/*  CREATE A PACKET VARIABLE */
	static uart_pkt_t packet;
	packet.payload.p_data = payload;
	packet.payload.data_len = PING_PACKET_SIZE;
	packet.type = uart_ping;
#else
	static uart_pkt_t packet;
	packet.payload.p_data = NULL;
	packet.payload.data_len = PING_PACKET_SIZE;
	packet.type = uart_ping;
#endif
	/*  SEND THE PACKET  */
	uart_util_send_pkt(&packet);
}

void send_ping_payload(uint8_t ping_payload) {
    static uart_pkt_t ping_packet;
    static uint8_t buf[1];
    buf[0] = ping_payload;
    ping_packet.payload.p_data = buf;
    ping_packet.payload.data_len = 1;
    ping_packet.type = uart_ping;
    uart_util_send_pkt(&ping_packet);
}

static void send_pong(uart_pkt_t* p_packet) {
	if(p_packet->payload.data_len ){
		static uint8_t pong_payload[MAX_PING_PAYLOAD];
		if(p_packet->payload.data_len > MAX_PING_PAYLOAD){
			p_packet->payload.data_len = MAX_PING_PAYLOAD;
		}
		memcpy(pong_payload, p_packet->payload.p_data, p_packet->payload.data_len);

		static uart_pkt_t pong_packet;
		pong_packet.payload.p_data = pong_payload;
		pong_packet.payload.data_len = p_packet->payload.data_len;
		pong_packet.type = uart_pong;

		uart_util_send_pkt(&pong_packet);
	}else{
		static uart_pkt_t pong_packet;
		pong_packet.payload.p_data = NULL;
		pong_packet.payload.data_len = p_packet->payload.data_len;
		pong_packet.type = uart_pong;

		uart_util_send_pkt(&pong_packet);
	}
}

void send_set_bt_scan_on(void) {
	static uart_pkt_t pong_packet;
	static uint8_t payload[1];
	pong_packet.payload.p_data = payload;
	pong_packet.payload.data_len = 1;
	pong_packet.type = uart_set_bt_scan_state;

	payload[0] = 1;

	uart_util_send_pkt(&pong_packet);
}

void send_set_bt_scan_off(void) {
	printf("Turning bt off\n");
	static uart_pkt_t pong_packet;
	static uint8_t payload[1];
	pong_packet.payload.p_data = payload;
	pong_packet.payload.data_len = 1;
	pong_packet.type = uart_set_bt_scan_state;

	payload[0] = 0;

	uart_util_send_pkt(&pong_packet);
}

void send_set_bt_scan_params(void) {
	static uart_pkt_t pong_packet;
	static uint8_t payload[7];
	pong_packet.payload.p_data = payload;
	pong_packet.payload.data_len = 7;
	pong_packet.type = uart_set_bt_scan_params;

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

	static char version_string[] = STRINGIFY(VERSION_STRING);

	static uart_pkt_t pong_packet;
	pong_packet.payload.p_data = (uint8_t*)version_string;
	pong_packet.payload.data_len = strlen(version_string);
	pong_packet.type = uart_evt_ready;

	uart_util_send_pkt(&pong_packet);
}

void send_reset(void) {
	static uart_pkt_t packet;
	packet.payload.p_data = NULL;
	packet.payload.data_len = 0;
	packet.type = uart_req_reset;

	uart_util_send_pkt(&packet);
}

void request_periodic_report_to_nordic(void){
	static uint8_t payload[4];
	payload[0] = report_timeout_ms >> 24;
	payload[1] = report_timeout_ms >> 16;
	payload[2] = report_timeout_ms >> 8;
	payload[3] = report_timeout_ms;


	static uart_pkt_t packet;
	packet.payload.p_data = payload;
	packet.payload.data_len = 4;
	packet.type = uart_req_bt_neigh_rep;

	uart_util_send_pkt(&packet);
#if NORDIC_WATCHDOG_ENABLED //turn this to one to enable the nordic watchdog
	nordic_watchdog_timeout_ms = (report_timeout_ms+100);
	ctimer_set(&m_nordic_watchdog_timer, (nordic_watchdog_timeout_ms*CLOCK_SECOND)/1000, nordic_watchdog_handler, NULL);
#endif
}

#define MAX_ATTEMPTS 3
void uart_util_ack_error(ack_wait_t* ack_wait_data) {
    printf("Uart ack error for packet: 0x%04X\n", (uint16_t)ack_wait_data->waiting_ack_for_pkt_type);
//	leds_on(LEDS_RED);
//	if(m_app_state == wait){
//		return;
//	}

	if(no_of_attempts < MAX_ATTEMPTS){
		no_of_attempts++;
		procedure_retry();
//		fsm_state_process(); //if there is an error try to resend
	}else{
//		fsm_stop();
		is_nordic_ready=false;
		reset_nodric(); //when the nordic will reboot it will generate the uart_ready evt. When the TI receives it, it restart the FSM
	}
}

void uart_util_ack_tx_done(void){
}

void uart_util_rx_handler(uart_pkt_t* p_packet) { //once it arrives here the ack is already sent

	static uart_pkt_type_t type;
	type = p_packet->type;
	static uint8_t *p_payload_data;
	p_payload_data = p_packet->payload.p_data;
	static uint32_t ack_value;

	switch (type) {
	case uart_app_level_ack:
		if (p_packet->payload.data_len == 5) {
			if (p_payload_data[0] == APP_ACK_SUCCESS) {//if the app ack is positive go on with fsm
				//leds_off(LEDS_RED);
				no_of_attempts = 1;
				procedure_step_on();
			} else {	//otherwise call the ack error handler
				uart_util_ack_error(NULL); //attention, this function can be called also from uart_util when the timeout for ack expires
			}
		}
		return; //this avoids sending ack for ack messages
		break;
	case uart_evt_ready:
		uart_util_send_ack(p_packet, APP_ACK_SUCCESS); //ack message for evt_ready can be avoided
		is_nordic_ready=true;
		return;  //ready message may not send ack
		break;
	case uart_req_reset:
		ack_value = APP_ERROR_NOT_IMPLEMENTED;
//		sd_nvic_SystemReset(fsm_stop);
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
	    uart_util_send_ack(p_packet, APP_ACK_SUCCESS); //ack message for evt_ready can be avoided
	    //      //here we shall wait the previous message to be sent, but if the uart buffers are enough long the two messages will fit together
	    if(p_packet->payload.data_len == 1) {
	    printf("Received pong from nordic, sending it to sink\n");
//		printf("Pong payload:\n");
//		int i;
//		for(i=0;i<p_packet->payload.data_len;i++){
//		    printf("[%u] %u\n",i,p_packet->payload.p_data[i]);
//
//		}
		static uint8_t data[1];
		data[0] = p_packet->payload.p_data[0];
		process_post(&vela_sender_process, event_pong_received, data);

		//		fsm_start();
	    }
		return;
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
	PROCESS_BEGIN();
	    event_data_ready = process_alloc_event();
        event_ping_requested = process_alloc_event();
        event_pong_received = process_alloc_event();
        event_nordic_message_received = process_alloc_event();
        turn_bt_on = process_alloc_event();
        turn_bt_off = process_alloc_event();
        turn_bt_on_w_params = process_alloc_event();

		cc26xx_uart_set_input(serial_line_input_byte);

		clock_wait(CLOCK_SECOND/4); //wait to send all the contiki text before enabling the uart flow control

		initialize_reset_pin();

		uart_util_initialize();

		reset_nodric();

		printf("Nordic board reset, waiting ready message\n");

		while (1) {
		    //send_ping();
			PROCESS_WAIT_EVENT();

			if (ev == serial_line_event_message) { //NB: *data contains a string, the '\n' character IS NOT included, the '\0' character IS included
				process_uart_rx_data((uint8_t*)data);
			}

			if(is_nordic_ready && !procedure_is_running()){  //accept events only after the nordic is ready
                if (ev == event_ping_requested) {
                    uint8_t* payload = (uint8_t*)data;
                    send_ping_payload(*payload);
                }
                if(ev == turn_bt_off) {
                    send_set_bt_scan_off();
                }
                if(ev == turn_bt_on) {
                    send_set_bt_scan_on();
                }
                if(ev == turn_bt_on_w_params) {

                    active_scan = ((uint8_t*)data)[0];     //boolean
                    scan_interval = ((((uint8_t*)data)[1])<<8)+((uint8_t*)data)[2];       /**< Scan interval between 0x0004 and 0x4000 in 0.625 ms units (2.5 ms to 10.24 s). */
                    scan_window = ((((uint8_t*)data)[3])<<8)+((uint8_t*)data)[4];     /**< Scan window between 0x0004 and 0x4000 in 0.625 ms units (2.5 ms to 10.24 s). */
                    timeout = ((((uint8_t*)data)[5])<<8)+((uint8_t*)data)[6];
                    report_timeout_ms = ((((uint8_t*)data)[7])<<24)+((((uint8_t*)data)[8])<<16)+((((uint8_t*)data)[9])<<8)+((uint8_t*)data)[10];

                    procedure_start(&bluetooth_on_procedure);
                }
			}
		}
	PROCESS_END();
}
/*---------------------------------------------------------------------------*/
/**
 * @}
 * @}
 */
