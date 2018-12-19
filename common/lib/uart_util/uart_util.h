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


#ifndef UART_UTIL_H__

#define UART_UTIL_H__

#include <stdint.h>
#ifndef CONTIKI
#include "nrf_gpio.h"
//#include "app_uart.h"
#include "nrf_serial.h"
#include "boards.h"
#endif

#define SERIAL_FIFOS_SIZE 128                         /**< UART TX buffer (FIFO) size. */

#define FIRST_PCB_WORKAROUND

#ifndef CONTIKI
	#define SERIAL_FIFO_TX_SIZE SERIAL_FIFOS_SIZE
	#define SERIAL_FIFO_RX_SIZE SERIAL_FIFOS_SIZE         /**< UART RX buffer (FIFO) size. */
	#define SERIAL_BUFF_TX_SIZE	64
	#define SERIAL_BUFF_RX_SIZE	1

	#ifdef REDIRECT_TO_USB
		#define UART_RX_PIN_NUMBER 				RX_PIN_NUMBER
		#define UART_TX_PIN_NUMBER 				TX_PIN_NUMBER
		#define UART_RTS_PIN_NUMBER 			RTS_PIN_NUMBER
		#define UART_CTS_PIN_NUMBER 			CTS_PIN_NUMBER
		#define UART_FLOW_CONTROL 				NRF_UART_HWFC_ENABLED//APP_UART_FLOW_CONTROL_DISABLED
		#define UART_PARITY_ENABLED 			NRF_UART_PARITY_EXCLUDED
		#define UART_BAUDRATE 					UART_BAUDRATE_BAUDRATE_Baud1M
	#else
#ifdef FIRST_PCB_WORKAROUND
		#define UART_RX_PIN_NUMBER 				NRF_GPIO_PIN_MAP(0,25)
		#define UART_TX_PIN_NUMBER 				NRF_GPIO_PIN_MAP(0,24)
#else
        #define UART_RX_PIN_NUMBER              NRF_GPIO_PIN_MAP(0,24)//TX_PIN_NUMBER//NRF_GPIO_PIN_MAP(0,24)
        #define UART_TX_PIN_NUMBER              NRF_GPIO_PIN_MAP(0,25)
#endif
		#define UART_RTS_PIN_NUMBER 			NRF_GPIO_PIN_MAP(0,23)
		#define UART_CTS_PIN_NUMBER 			NRF_GPIO_PIN_MAP(0,22)
		#define UART_FLOW_CONTROL 				NRF_UART_HWFC_ENABLED//NRF_UART_HWFC_ENABLED
		#define UART_PARITY_ENABLED 			NRF_UART_PARITY_EXCLUDED
		#define UART_BAUDRATE 					UART_BAUDRATE_BAUDRATE_Baud1M //NB: check the baudrate, some settings are nonstandard. See UART_BAUDRATE_BAUDRATE_Baud921600 in nrf52_bitfields.h
	#endif
#else	//CONTIKI
	#include <stddef.h>
#endif

#define UART_PKT_START_SYMBOL   		'\x02' 		//start of text
#define UART_PKT_END_SYMBOL   			'\n'		//new line

#ifndef CONTIKI
	#define UART_FRAME_MAX_LEN_BYTE				SERIAL_FIFO_TX_SIZE
#else
	#ifdef SERIAL_LINE_CONF_BUFSIZE
		#if SERIAL_FIFOS_SIZE != SERIAL_LINE_CONF_BUFSIZE
			error("SERIAL_FIFOS_SIZE and SERIAL_FIFO_TX_SIZE must be set to the same value. Look into project makefile.");
		#endif
	#else
		#define SERIAL_LINE_CONF_BUFSIZE		SERIAL_FIFOS_SIZE
	#endif //SERIAL_LINE_CONF_BUFSIZE
	#define UART_FRAME_MAX_LEN_BYTE				SERIAL_LINE_CONF_BUFSIZE
#endif //!CONTIKI

#define UART_FRAME_START_SEQ_LEN_BYTE	1		//START SEQUENCE (0x02) LENGTH - this length is in byte.
#define UART_FRAME_END_SEQ_LEN_BYTE		1		//START SEQUENCE (0x02) LENGTH - this length is in byte. 0x** is long 1

#define HEX_FRAME_LEN_LEN_BYTE			4		//Length in bytes (remember that the protocol use codings of bytes into hex, every symbol is two bytes long)
#define UART_PKT_TYPE_LEN_BYTE			4		//Length in bytes (remember that the protocol use codings of bytes into hex, every symbol is two bytes long)
#define UART_PKT_HEX_FRAME_MAX_LEN_BYTE	(uint16_t) (UART_FRAME_MAX_LEN_BYTE - UART_FRAME_START_SEQ_LEN_BYTE - UART_FRAME_END_SEQ_LEN_BYTE) //Length in bytes (remember that the protocol use codings of bytes into hex, every symbol is two bytes long)
#define UART_PKT_PAYLOAD_MAX_LEN_BYTE	(uint16_t) (UART_PKT_HEX_FRAME_MAX_LEN_BYTE - HEX_FRAME_LEN_LEN_BYTE - UART_PKT_TYPE_LEN_BYTE)     //Length in bytes (remember that the protocol use codings of bytes into hex, every symbol is two bytes long)

#define UART_FRAME_MAX_LEN_SYMB			UART_FRAME_MAX_LEN_BYTE / 2
#define HEX_FRAME_LEN_LEN_SYMB			HEX_FRAME_LEN_LEN_BYTE / 2			//Length in symbols (remember that the protocol use codings of bytes into hex, every symbol is two bytes long)
#define UART_PKT_TYPE_LEN_SYMB			UART_PKT_TYPE_LEN_BYTE / 2			//Length in symbols (remember that the protocol use codings of bytes into hex, every symbol is two bytes long)
#define UART_PKT_HEX_FRAME_MAX_LEN_SYMB	UART_PKT_HEX_FRAME_MAX_LEN_BYTE / 2	//Length in symbols (remember that the protocol use codings of bytes into hex, every symbol is two bytes long)
#define UART_PKT_PAYLOAD_MAX_LEN_SYMB	UART_PKT_PAYLOAD_MAX_LEN_BYTE / 2	//Length in symbols (remember that the protocol use codings of bytes into hex, every symbol is two bytes long)


#ifdef CONTIKI
#define NRF_ERROR_BASE_NUM      (0x0)       ///< Global error base
#define NRF_SUCCESS                           (NRF_ERROR_BASE_NUM + 0)  ///< Successful command
#define NRF_ERROR_SVC_HANDLER_MISSING         (NRF_ERROR_BASE_NUM + 1)  ///< SVC handler is missing
#define NRF_ERROR_SOFTDEVICE_NOT_ENABLED      (NRF_ERROR_BASE_NUM + 2)  ///< SoftDevice has not been enabled
#define NRF_ERROR_INTERNAL                    (NRF_ERROR_BASE_NUM + 3)  ///< Internal Error
#define NRF_ERROR_NO_MEM                      (NRF_ERROR_BASE_NUM + 4)  ///< No Memory for operation
#define NRF_ERROR_NOT_FOUND                   (NRF_ERROR_BASE_NUM + 5)  ///< Not found
#define NRF_ERROR_NOT_SUPPORTED               (NRF_ERROR_BASE_NUM + 6)  ///< Not supported
#define NRF_ERROR_INVALID_PARAM               (NRF_ERROR_BASE_NUM + 7)  ///< Invalid Parameter
#define NRF_ERROR_INVALID_STATE               (NRF_ERROR_BASE_NUM + 8)  ///< Invalid state, operation disallowed in this state
#define NRF_ERROR_INVALID_LENGTH              (NRF_ERROR_BASE_NUM + 9)  ///< Invalid Length
#define NRF_ERROR_INVALID_FLAGS               (NRF_ERROR_BASE_NUM + 10) ///< Invalid Flags
#define NRF_ERROR_INVALID_DATA                (NRF_ERROR_BASE_NUM + 11) ///< Invalid Data
#define NRF_ERROR_DATA_SIZE                   (NRF_ERROR_BASE_NUM + 12) ///< Data size exceeds limit
#define NRF_ERROR_TIMEOUT                     (NRF_ERROR_BASE_NUM + 13) ///< Operation timed out
#define NRF_ERROR_NULL                        (NRF_ERROR_BASE_NUM + 14) ///< Null Pointer
#define NRF_ERROR_FORBIDDEN                   (NRF_ERROR_BASE_NUM + 15) ///< Forbidden Operation
#define NRF_ERROR_INVALID_ADDR                (NRF_ERROR_BASE_NUM + 16) ///< Bad Memory Address
#define NRF_ERROR_BUSY                        (NRF_ERROR_BASE_NUM + 17) ///< Busy
#endif

#define APP_ACK_SUCCESS							NRF_SUCCESS
#define APP_ERROR_GENERIC						NRF_ERROR_INTERNAL
#define APP_ERROR_NO_MEM						NRF_ERROR_NO_MEM
#define APP_ERROR_COMMAND_NOT_VALID				NRF_ERROR_NOT_SUPPORTED
#define APP_ERROR_NOT_IMPLEMENTED				NRF_ERROR_NOT_FOUND
#define APP_ERROR_NOT_FOUND						NRF_ERROR_NOT_FOUND
#define APP_ERROR_INVALID_STATE					NRF_ERROR_INVALID_STATE

typedef enum{
	uart_app_level_ack 				= 0x0001,
	uart_evt_ready					= 0x0010,

	uart_req_reset 					= 0x0100,

	uart_req_bt_state				= 0x0140,
	uart_req_bt_scan_state			= 0x0141,
	uart_req_bt_adv_state			= 0x0142,

	uart_req_bt_scan_params			= 0x0151,
	uart_req_bt_adv_params			= 0x0152,

	uart_req_bt_neigh_rep_format	= 0x0180,
	uart_req_bt_neigh_rep 			= 0x0181,


	uart_resp_bt_state				= 0x0240,
	uart_resp_bt_scan_state			= 0x0241,
	uart_resp_bt_adv_state			= 0x0242,

	uart_resp_bt_scan_params		= 0x0251,
	uart_resp_bt_adv_params			= 0x0252,

	uart_resp_bt_neigh_rep_format	= 0x0280,
	uart_resp_bt_neigh_rep_start	= 0x0281,
	uart_resp_bt_neigh_rep_more 	= 0x0282,
	uart_resp_bt_neigh_rep_end  	= 0x0283,


	uart_set_bt_state				= 0x0400,
	uart_set_bt_scan_state			= 0x0401,
	uart_set_bt_adv_state			= 0x0403,

	uart_set_bt_scan_params			= 0x0411,
	uart_set_bt_adv_params			= 0x0412,


	uart_ping						= 0xFF00,
	uart_pong						= 0xFF01,
	//ADD: whoAmI, temperature
} uart_pkt_type_t;

/**@brief Variable length data encapsulation in terms of length and pointer to data. */
typedef struct
{
    uint8_t  * p_data;      /**< Pointer to data. */
    uint16_t   data_len;    /**< Length of data. */
} payload_t;



/**@brief Variable length data encapsulation in terms of length and pointer to data. */
// ALM : moved the definiton to the uart.h inside the network_board
//TODO: use just one of the two struct typedef (payload_t and data_t are the same)
//typedef struct
//{
//    uint8_t  * p_data;      /**< Pointer to data. */
//    uint16_t   data_len;    /**< Length of data. */
//} data_t;

typedef struct{
	uart_pkt_type_t  type;
	payload_t		 payload;
} uart_pkt_t;


typedef struct{
	uint8_t ack_waiting;
	uint32_t ack_wait_timeout_ticks;
	uint32_t ack_wait_start_ticks;
	uart_pkt_type_t waiting_ack_for_pkt_type;
	uint16_t waiting_ack_for_pkt_length;
} ack_wait_t;


uint32_t uart_util_send_pkt(uart_pkt_t* uart_pkt_t);

void uart_util_initialize(void);

extern void uart_util_rx_handler(uart_pkt_t* p_packet);
extern void uart_util_ack_error(ack_wait_t* ack_wait_data);
extern void uart_util_ack_tx_done(void);

void uart_util_flush(void);
uint8_t to_hex(uint8_t i);
void uart_util_send_ack(uart_pkt_t* p_packet, uint8_t error_code, uint32_t ack_delay_us);
uint8_t uart_util_is_waiting_ack(void);

#ifdef CONTIKI
void process_uart_rx_data(uint8_t *serial_data);
#endif
//void uart_util_reset_fifo(void);


#endif
