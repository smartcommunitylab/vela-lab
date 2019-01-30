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
#include "sequential_procedures.h"

#define NORDIC_WATCHDOG_ENABLED						1

#ifdef DEBUG
#undef DEBUG
#endif
#define DEBUG 1
#if DEBUG
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

PROCESS(cc2650_uart_process, "cc2650 uart process");

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
#define DEFAULT_REPORT_TIMEOUT_MS   60000
#define UART_ACK_DELAY_US           2000

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
static uint32_t report_timeout_ms = 0, report_timeout_ms_arg = 0;
static uint8_t bt_scan_state = 0;

static uint32_t report_ready(data_t *p_data);
static uint32_t report_rx_handler(uart_pkt_t* p_packet, report_type_t m_report_type );

static void ping_timeout_handler(void *ptr);
#if NORDIC_WATCHDOG_ENABLED
static void nordic_watchdog_handler(void *ptr);
#endif

void reset_nodric(void);

static uint8_t send_ping(void);
static uint8_t send_set_bt_scan(void);
static uint8_t send_set_bt_scan_on(void);
static uint8_t send_set_bt_scan_off(void);
static uint8_t send_set_bt_scan_params(void);
static uint8_t send_ready(void);
static uint8_t send_reset(void);
static uint8_t request_periodic_report_to_nordic(void);
static uint8_t stop_periodic_report_to_nordic(void);

void uart_util_rx_handler(uart_pkt_t* p_packet);
void uart_util_ack_tx_done(void);
extern void uart_util_ack_error(ack_wait_t* ack_wait_data);

PROCEDURE(bluetooth_on, &send_set_bt_scan_params, &send_set_bt_scan_on, &request_periodic_report_to_nordic);
PROCEDURE(bluetooth_off, &send_set_bt_scan_off, &stop_periodic_report_to_nordic);
PROCEDURE(ready,&send_ready);
PROCEDURE(ready_plus_last_bt_conf,&send_ready,&send_set_bt_scan_params, &send_set_bt_scan, &request_periodic_report_to_nordic);

//ALM -- called to start this as a contiki THREAD
void vela_uart_init() {
  // start the uart process
  process_start(&cc2650_uart_process, "cc2650 uart process");
  return;

}


static uint32_t report_ready(data_t *p_data){
	//PROCESS REPORT: return APP_ACK_SUCCESS as soon as possible
	process_post(&vela_sender_process, event_data_ready, p_data);
#if NORDIC_WATCHDOG_ENABLED
	nordic_watchdog_value = 0; //reset the nordic watchdog
#endif
	return APP_ACK_SUCCESS;
}
static uint16_t buff_free_from = 0;
static uint32_t ret = APP_ERROR_COMMAND_NOT_VALID;

static uint32_t report_rx_handler(uart_pkt_t* p_packet, report_type_t m_report_type ){

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

static void ping_timeout_handler(void *ptr){
	ctimer_reset(&m_ping_timer);

	send_ping();
}

#if NORDIC_WATCHDOG_ENABLED
static void nordic_watchdog_handler(void *ptr){
	nordic_watchdog_value++;

	if(nordic_watchdog_value > 3){
	    PRINTF("Nordic didn't respond, resetting it!\n");
	    reset_nodric();
//		while(1){ //Stay here. The reset of the mcu will be triggered by the watchdog timer initialized into contiki-main.c
//		}
	}else{
		if(nordic_watchdog_timeout_ms != 0){
			ctimer_set(&m_nordic_watchdog_timer, (nordic_watchdog_timeout_ms*CLOCK_SECOND)/1000, nordic_watchdog_handler, NULL);
		}
	}
}
#endif

static uint8_t start_procedure(procedure_t *m_procedure){
    return sequential_procedure_start(m_procedure, uart_util_is_waiting_ack()); //if we are waiting an ack start use the delayed start
}

#define RESET_PIN_IOID			BOARD_IOID_DIO22
void reset_nodric(void){
	GPIO_clearDio(RESET_PIN_IOID);
	clock_wait(CLOCK_SECOND/10);
	GPIO_setDio(RESET_PIN_IOID);
	clock_wait(CLOCK_SECOND/10);
}


//example of use of uart_util_send. This function is called with a timer and send a packet to the UART
static uint8_t send_ping(void) {
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

	return 0;
}

static void send_ping_payload(uint8_t ping_payload) {
    static uart_pkt_t ping_packet;
    static uint8_t buf[1];
    buf[0] = ping_payload;
    ping_packet.payload.p_data = buf;
    ping_packet.payload.data_len = 1;
    ping_packet.type = uart_ping;
    uart_util_send_pkt(&ping_packet);
}

static uint8_t send_pong(uart_pkt_t* p_packet) {
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
    return 0;
}

static uint8_t send_set_bt_scan(void) {
    static uart_pkt_t packet;
    static uint8_t payload[1];
    packet.payload.p_data = payload;
    packet.payload.data_len = 1;
    packet.type = uart_set_bt_scan_state;

    payload[0] = bt_scan_state;

    uart_util_send_pkt(&packet);

    return 0;
}


static uint8_t send_set_bt_scan_on(void) {
    bt_scan_state=1;

	return send_set_bt_scan();
}

static uint8_t send_set_bt_scan_off(void) {
	PRINTF("Turning bt off\n");
	bt_scan_state=0;

    return send_set_bt_scan();
}

static uint8_t send_set_bt_scan_params(void) {
	static uart_pkt_t packet;
	static uint8_t payload[7];
	packet.payload.p_data = payload;
	packet.payload.data_len = 7;
	packet.type = uart_set_bt_scan_params;

	payload[0] = active_scan; //active_scan
	payload[1] = scan_interval >> 8;
	payload[2] = scan_interval;
	payload[3] = scan_window >> 8;
	payload[4] = scan_window;
	payload[5] = timeout >> 8;
	payload[6] = timeout;

	uart_util_send_pkt(&packet);

    return 0;
}

static uint8_t send_ready(void) {

	static char version_string[] = STRINGIFY(VERSION_STRING);

	static uart_pkt_t packet;
	packet.payload.p_data = (uint8_t*)version_string;
	packet.payload.data_len = strlen(version_string);
	packet.type = uart_evt_ready;

	uart_util_send_pkt(&packet);

    return 0;
}

static uint8_t send_reset(void) {
	static uart_pkt_t packet;
	packet.payload.p_data = NULL;
	packet.payload.data_len = 0;
	packet.type = uart_req_reset;

	uart_util_send_pkt(&packet);

    return 0;
}

static uint8_t request_periodic_report_to_nordic(void){
	static uint8_t payload[4];
	payload[0] = report_timeout_ms_arg >> 24;
	payload[1] = report_timeout_ms_arg >> 16;
	payload[2] = report_timeout_ms_arg >> 8;
	payload[3] = report_timeout_ms_arg;


	static uart_pkt_t packet;
	packet.payload.p_data = payload;
	packet.payload.data_len = 4;
	packet.type = uart_req_bt_neigh_rep;

	uart_util_send_pkt(&packet);
#if NORDIC_WATCHDOG_ENABLED //turn this to one to enable the nordic watchdog
	nordic_watchdog_timeout_ms = (report_timeout_ms_arg+100);
	if(report_timeout_ms_arg!=0){
	    ctimer_set(&m_nordic_watchdog_timer, (nordic_watchdog_timeout_ms*CLOCK_SECOND)/1000, nordic_watchdog_handler, NULL);
	}else{
	    nordic_watchdog_timeout_ms=0;
	    ctimer_stop(&m_nordic_watchdog_timer);
	}
#endif

    return 0;
}

static uint8_t stop_periodic_report_to_nordic(void){
    report_timeout_ms_arg=0;
    request_periodic_report_to_nordic();

    return 0;
}

#define MAX_ATTEMPTS 3
void uart_util_ack_error(ack_wait_t* ack_wait_data) {
    if(ack_wait_data != NULL){
        PRINTF("Uart ack error for packet: 0x%04X\n", (uint16_t)ack_wait_data->waiting_ack_for_pkt_type);
    }else{
        PRINTF("Uart ack error\n");
    }

	if(no_of_attempts < MAX_ATTEMPTS){
		no_of_attempts++;
		sequential_procedure_retry();
	}else{
	    PRINTF("Nordic didn't respond, resetting it!\n");
		is_nordic_ready=false;
		reset_nodric(); //when the nordic will reboot it will generate the uart_ready evt. When the TI receives it, it restart the FSM
	}
}

void uart_util_ack_tx_done(void){
}

static uint32_t pre_ack_uart_rx_handler(uart_pkt_t* p_packet) {
    uart_pkt_type_t type;
    type = p_packet->type;
    uint8_t *p_payload_data;
    p_payload_data = p_packet->payload.p_data;
    uint32_t ack_value;

    switch (type) {
    case uart_app_level_ack:
        if (p_packet->payload.data_len == 5) {
            ack_value = APP_ACK_SUCCESS;
        }else{
            ack_value = APP_ERROR_COMMAND_NOT_VALID;
        }
        break;
    case uart_evt_ready:
        ack_value = APP_ACK_SUCCESS;
//        return;
        break;
    case uart_req_reset:
        ack_value = APP_ERROR_NOT_IMPLEMENTED;
//      sd_nvic_SystemReset(fsm_stop);
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
        break;
    case uart_ping:
        ack_value = APP_ACK_SUCCESS;
        break;
    case uart_pong:
        ack_value = APP_ACK_SUCCESS;
        break;
    default:
        ack_value = APP_ERROR_NOT_FOUND;
        break;
    }

    return ack_value;
}

static void post_ack_uart_rx_handler(uart_pkt_t* p_packet) {
    uart_pkt_type_t type;
    type = p_packet->type;
    uint8_t *p_payload_data;
    p_payload_data = p_packet->payload.p_data;

    switch (type) {
    case uart_app_level_ack:
        if (p_packet->payload.data_len == 5) {
            if (p_payload_data[0] == APP_ACK_SUCCESS) {
                no_of_attempts = 1;
                sequential_procedure_execute_action();
            } else {
                uart_util_ack_error(NULL); //attention, this function can be called also from uart_util when the timeout for ack expires
            }
        }
        break;
    case uart_evt_ready:
        is_nordic_ready=true;
//        start_procedure(&ready);
        start_procedure(&ready_plus_last_bt_conf); //this procedure doesn't work with too many printf enabled
        break;
    case uart_req_reset:
    case uart_req_bt_state:
    case uart_req_bt_scan_state:
    case uart_req_bt_adv_state:
    case uart_req_bt_scan_params:
    case uart_req_bt_adv_params:
    case uart_req_bt_neigh_rep:
    case uart_resp_bt_state:
    case uart_resp_bt_scan_state:
    case uart_resp_bt_adv_state:
    case uart_resp_bt_scan_params:
    case uart_resp_bt_adv_params:
    case uart_resp_bt_neigh_rep_start:
    case uart_resp_bt_neigh_rep_more:
        break;
    case uart_resp_bt_neigh_rep_end:
        break;
    case uart_set_bt_state:
    case uart_set_bt_scan_state:
    case uart_set_bt_adv_state:
    case uart_set_bt_scan_params:
    case uart_set_bt_adv_params:
        break;
    case uart_ping: //here we shall wait the previous message to be sent, but if the buffers are enough long the two messages will fit
        send_pong(p_packet);
        break;
    case uart_pong:
        if (p_packet->payload.data_len == 1)
        {
            PRINTF("Received pong from nordic, sending it to sink\n");
            static uint8_t data[1];
            data[0] = p_packet->payload.p_data[0];
            process_post(&vela_sender_process, event_pong_received, data);
        }
        break;
    default:
        break;
    }

    return;
}

void uart_util_rx_handler(uart_pkt_t* p_packet) {

	static uint32_t ack_value;

	ack_value = pre_ack_uart_rx_handler(p_packet);

	if(p_packet->type != uart_app_level_ack){ //do not send the ack for acks
	    uart_util_send_ack(p_packet, ack_value,UART_ACK_DELAY_US);
	}

	post_ack_uart_rx_handler(p_packet);

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
        turn_bt_on_low = process_alloc_event();
        turn_bt_on_def = process_alloc_event();
        turn_bt_on_high = process_alloc_event();
        reset_bt = process_alloc_event();

		cc26xx_uart_set_input(serial_line_input_byte);

		clock_wait(CLOCK_SECOND/4); //wait to send all the contiki text before enabling the uart flow control

		initialize_reset_pin();

		uart_util_initialize();

		reset_nodric();
		//TODO: if the ready message isn't received within a timeout the reset should be redone till the message is received.
		//Once the ready message is received the boot is completed

		PRINTF("Nordic board reset, waiting ready message\n");

		while (1) {
		    //send_ping();
			PROCESS_WAIT_EVENT();

			if (ev == serial_line_event_message) { //NB: *data contains a string, the '\n' character IS NOT included, the '\0' character IS included
				process_uart_rx_data((uint8_t*)data);
			}

			if(!sequential_procedure_is_running()){
                if (ev == event_ping_requested) {
                    uint8_t* payload = (uint8_t*)data;
                    send_ping_payload(*payload);
                }
                if(ev == turn_bt_off) {
                    start_procedure(&bluetooth_off);
                }
                if(ev == turn_bt_on) {
                    if(report_timeout_ms == 0){   //turn_bt_on is sent before any call to turn_bt_on_w_params. Load default value
                        scan_window = DEFAULT_SCAN_WINDOW;
                        scan_interval = DEFAULT_SCAN_INTERVAL;
                        report_timeout_ms = DEFAULT_REPORT_TIMEOUT_MS;
                    }
                    report_timeout_ms_arg = report_timeout_ms;
                    start_procedure(&bluetooth_on);
                }
                if(ev == turn_bt_on_w_params) {

                    active_scan = ((uint8_t*)data)[0];     //boolean
                    scan_interval = ((((uint8_t*)data)[1])<<8)+((uint8_t*)data)[2];       /**< Scan interval between 0x0004 and 0x4000 in 0.625 ms units (2.5 ms to 10.24 s). */
                    scan_window = ((((uint8_t*)data)[3])<<8)+((uint8_t*)data)[4];     /**< Scan window between 0x0004 and 0x4000 in 0.625 ms units (2.5 ms to 10.24 s). */
                    timeout = ((((uint8_t*)data)[5])<<8)+((uint8_t*)data)[6];
                    report_timeout_ms = ((((uint8_t*)data)[7])<<24)+((((uint8_t*)data)[8])<<16)+((((uint8_t*)data)[9])<<8)+((uint8_t*)data)[10];
                    report_timeout_ms_arg = report_timeout_ms;

                    start_procedure(&bluetooth_on);
                }
                if(ev == turn_bt_on_def) {
                    scan_window = DEFAULT_SCAN_WINDOW;     /**< Scan window between 0x0004 and 0x4000 in 0.625 ms units (2.5 ms to 10.24 s). */
                    scan_interval = DEFAULT_SCAN_INTERVAL;     /**< Scan window between 0x0004 and 0x4000 in 0.625 ms units (2.5 ms to 10.24 s). */
                    report_timeout_ms_arg = DEFAULT_REPORT_TIMEOUT_MS;
                    report_timeout_ms = DEFAULT_REPORT_TIMEOUT_MS;
                    start_procedure(&bluetooth_on);
                }
                if(ev == reset_bt){
#if NORDIC_WATCHDOG_ENABLED
                    ctimer_stop(&m_nordic_watchdog_timer);
#endif
                    reset_nodric();
                }

                //next commands are deprecated on the gateway but still handled by the TI board
                if(ev == turn_bt_on_low) {
                    scan_window = DEFAULT_SCAN_WINDOW / 2;     /**< Scan window between 0x0004 and 0x4000 in 0.625 ms units (2.5 ms to 10.24 s). */
                    report_timeout_ms_arg = report_timeout_ms;
                    start_procedure(&bluetooth_on);
                }
                if(ev == turn_bt_on_high) {
                    scan_window = DEFAULT_SCAN_INTERVAL;     /**< Scan window between 0x0004 and 0x4000 in 0.625 ms units (2.5 ms to 10.24 s). */
                    report_timeout_ms_arg = report_timeout_ms;
                    start_procedure(&bluetooth_on);
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
