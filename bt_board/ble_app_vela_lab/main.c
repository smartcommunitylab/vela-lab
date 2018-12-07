/**
 * Copyright (c) 2016 - 2017, Nordic Semiconductor ASA
 * 
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form, except as embedded into a Nordic
 *    Semiconductor ASA integrated circuit in a product or a software update for
 *    such product, must reproduce the above copyright notice, this list of
 *    conditions and the following disclaimer in the documentation and/or other
 *    materials provided with the distribution.
 * 
 * 3. Neither the name of Nordic Semiconductor ASA nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 * 
 * 4. This software, with or without modification, must only be used with a
 *    Nordic Semiconductor ASA integrated circuit.
 * 
 * 5. Any software provided in binary form under this license must not be reverse
 *    engineered, decompiled, modified and/or disassembled.
 * 
 * THIS SOFTWARE IS PROVIDED BY NORDIC SEMICONDUCTOR ASA "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL NORDIC SEMICONDUCTOR ASA OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * 
 */
/**@cond To Make Doxygen skip documentation generation for this file.
 * @{
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>


#include "sdk_config.h"
//#include "boards.h"
#include "nrf.h"
#include "ble.h"
#include "ble_hci.h"
#include "nordic_common.h"
#include "nrf_gpio.h"
#include "bsp_btn_ble.h"
#include "ble_advdata.h"
#include "ble_srv_common.h"
#include "nrf_sdh.h"
#include "nrf_sdh_ble.h"
#include "nrf_ble_gatt.h"
#include "app_timer.h"
#include "app_error.h"
#include "nrf_soc.h"

//#include "nrf_cli.h"
//#include "nrf_cli_rtt.h"
//#include "nrf_cli_uart.h"

//#include "nrf_log.h"
//#include "nrf_log_ctrl.h"
//#include "nrf_log_default_backends.h"
#include "uart_util.h"
#include "constraints.h"

#ifdef ENABLE_TOF
#include "ifs_tof.h"
#endif
#define CONN_INTERVAL_DEFAULT           (uint16_t)(MSEC_TO_UNITS(10, UNIT_1_25_MS))    /**< Default connection interval used at connection establishment by central side. */

#define CONN_INTERVAL_MIN               (uint16_t)(MSEC_TO_UNITS(10, UNIT_1_25_MS))    /**< Minimum acceptable connection interval, in 1.25 ms units. */
#define CONN_INTERVAL_MAX               (uint16_t)(MSEC_TO_UNITS(25, UNIT_1_25_MS))    /**< Maximum acceptable connection interval, in 1.25 ms units. */
#define CONN_SUP_TIMEOUT                (uint16_t)(MSEC_TO_UNITS(100,  UNIT_10_MS))    /**< Connection supervisory timeout (4 seconds). */
#define SLAVE_LATENCY                   0                                               /**< Slave latency. */

#define READY_LED                       1 << BSP_BOARD_LED_0
#define PROGRESS_LED                    1 << BSP_BOARD_LED_1
#define SCAN_ADV_LED                    1 << BSP_BOARD_LED_2
#define ERROR_LED                       1 << BSP_BOARD_LED_3

#define LED_BLINK_TIMEOUT_MS			50
#define LP_LED_INTERVAL					5000

#define TOGGLE_SCAN_BUTTON             	BSP_BUTTON_2                                    /**< Button to press at beginning of the test to indicate that this board is connected to the PC and takes input from it via the UART. */
#define TOGGLE_ADV_BUTTON           	BSP_BUTTON_3                                    /**< Button to press at beginning of the test to indicate that this board is standalone (automatic behavior). */
//#define VERBOSE_LED_BUTTON				BSP_BUTTON_3
#define BUTTON_DETECTION_DELAY          APP_TIMER_TICKS(50)                             /**< Delay from a GPIOTE event until a button is reported as pushed (in number of timer ticks). */

#define APP_BLE_CONN_CFG_TAG            1                                               /**< A tag that refers to the BLE stack configuration. */
#define APP_BLE_OBSERVER_PRIO           1                                               /**< Application's BLE observer priority. You shouldn't need to modify this value. */
#define L2CAP_HDR_LEN                   4                                               /**< L2CAP header length. */

#define USE_NANOSECOND
#define nENABLE_PHY_UPDATE

#define MAXIMUM_NETWORK_SIZE			MAX_NUMBER_OF_BT_BEACONS//NRF_SDH_BLE_TOTAL_LINK_COUNT
#define MAXIMUM_NUMBER_OF_PERIPH_CONN	NRF_SDH_BLE_PERIPHERAL_LINK_COUNT
#define MAXIMUM_NUMBER_OF_CENTRAL_CONN	IFT_TOF_MAX_NUM_OF_CENTRAL_LINKS

#define ENABLE_AMTv

#define DEFAULT_ACTIVE_SCAN             1     //boolean
#define DEFAULT_SCAN_INTERVAL           3520       /**< Scan interval between 0x0004 and 0x4000 in 0.625 ms units (2.5 ms to 10.24 s). */
#define DEFAULT_SCAN_WINDOW             1920     /**< Scan window between 0x0004 and 0x4000 in 0.625 ms units (2.5 ms to 10.24 s). */
#define DEFAULT_TIMEOUT                 0
#define PERIODIC_TIMER_INTERVAL         APP_TIMER_TICKS(1000)
#define NODE_TIMEOUT_DEFAULT_MS			(uint32_t)60000

#define MIN_REPORT_TIMEOUT_MS					1000

#define APP_TIMER_MS(TICKS)  (uint32_t)(ROUNDED_DIV (((uint64_t)TICKS * ( ( (uint64_t)APP_TIMER_CONFIG_RTC_FREQUENCY + 1 ) * 1000 )) , (uint64_t)APP_TIMER_CLOCK_FREQ ))

APP_TIMER_DEF(m_periodic_timer_id);
APP_TIMER_DEF(m_report_timer_id);
APP_TIMER_DEF(m_led_blink_timer_id);
APP_TIMER_DEF(m_lp_led_timer_id);

typedef enum
{
    CLIMB_BEACON,
    EDDYSTONE_BEACON,
    NORDIC_BEACON,	//not used for now
	UNKNOWN_TYPE,
} node_type_t;

typedef enum
{
    NOT_SELECTED = 0x00,
    BOARD_TESTER,
    BOARD_DUMMY,
} board_role_t;

typedef struct
{
    uint16_t        att_mtu;                    /**< GATT ATT MTU, in bytes. */
    uint16_t        conn_interval;              /**< Connection interval expressed in units of 1.25 ms. */
    ble_gap_phys_t  phys;                       /**< Preferred PHYs. */
    bool            data_len_ext_enabled;       /**< Data length extension status. */
    bool            conn_evt_len_ext_enabled;   /**< Connection event length extension status. */
#ifdef ENABLE_AMT
    bool            transmit_data;				/**< Whether to transmit data during the test */
#endif
    bool 			continuous_test;			/**< if set to true the test keeps going on (works without data stream) */
} test_params_t;

typedef struct
{
    uint8_t    p_data[31];      /**< Pointer to data. */
    uint16_t   len;    /**< Length of data. */
} s_data_t;

typedef struct
{
    uint16_t  			conn_handle;
    uint8_t             local_role;
	ble_gap_addr_t 		bd_address;
	s_data_t			adv_data;
	s_data_t			scan_data;
	uint32_t			first_contact_ticks;
	uint32_t 			last_contact_ticks;
	int8_t				last_rssi;
	int8_t				max_rssi;
	uint8_t				beacon_msg_count;
	uint8_t				namespace_id[EDDYSTONE_NAMESPACE_ID_LENGTH];
	uint8_t				instance_id[EDDYSTONE_INSTANCE_ID_LENGTH];
	node_type_t			node_type;
} node_t;


#ifdef ENABLE_AMT
NRF_BLE_GATT_DEF(m_gatt);                   /**< GATT module instance. */
BLE_DB_DISCOVERY_DEF(m_ble_db_discovery);   /**< DB discovery module instance. */

static nrf_ble_amtc_t     m_amtc;
static nrf_ble_amts_t     m_amts;
NRF_SDH_BLE_OBSERVER(m_amtc_ble_obs, BLE_AMTC_BLE_OBSERVER_PRIO, nrf_ble_amtc_on_ble_evt, &m_amtc);
NRF_SDH_BLE_OBSERVER(m_amts_ble_obs, BLE_AMTS_BLE_OBSERVER_PRIO, nrf_ble_amts_on_ble_evt, &m_amts);
#endif

static bool volatile m_conn_interval_configured;

//static uint16_t m_conn_handle = BLE_CONN_HANDLE_INVALID;    /**< Handle of the current BLE connection .*/
//static uint8_t m_gap_role     = BLE_GAP_ROLE_INVALID;       /**< BLE role for this connection, see @ref BLE_GAP_ROLES */
//static ble_gap_addr_t m_remote_address;
// Name to use for advertising and connection.

static char const m_target_periph_name[] = DEVICE_NAME;
static char const m_target_periph_name_alt[] = {'C',
												'L',
												'I',
												'M',
												'B',
												'C'};

bool m_scan_active = false, m_advertising_active = false, m_ready_received = false, m_tx_error = false;
static uint32_t m_periodic_report_timeout = 0;
uint32_t report_procedure_running = 0;
uint8_t stop = 0;
uint32_t beacon_timeout_ms = NODE_TIMEOUT_DEFAULT_MS;
//uint32_t blinking_led_bit_map;

#ifdef ENABLE_TOF
static uint8_t m_no_of_central_links = 0;
static uint8_t m_no_of_peripheral_links = 0;
static ifs_tof_t m_ifs_tof[MAXIMUM_NUMBER_OF_CENTRAL_CONN];
static uint32_t ifs_offset_ticks;
static bool allow_new_connections = false;

int8_t rssi[TOF_SAMPLES_TO_ACQUIRE];
uint32_t frequency_reg[TOF_SAMPLES_TO_ACQUIRE];
uint16_t total_number_of_connection_events;
bool m_scan_active = false, m_advertising_active = false;


NRF_SDH_BLE_OBSERVER(m_ifs_tof_bel_obs, BLE_IFS_TOF_BLE_OBSERVER_PRIO, ifs_tof_on_ble_evt, NULL);
#endif

static node_t m_network[MAXIMUM_NETWORK_SIZE];

// Test parameters.
// Settings like ATT MTU size are set only once on the dummy board.
// Make sure that defaults are sensible.
static test_params_t m_test_params =
{
    .att_mtu                  = NRF_SDH_BLE_GATT_MAX_MTU_SIZE,
    .conn_interval            = CONN_INTERVAL_DEFAULT,
    .data_len_ext_enabled     = true,
    .conn_evt_len_ext_enabled = true,
#ifdef ENABLE_AMT
	.transmit_data			  = false,
#endif
	.continuous_test		  = true,
    // Only symmetric PHYs are supported.
#if defined(S140)
    .phys.tx_phys             = BLE_GAP_PHY_2MBPS | BLE_GAP_PHY_1MBPS | BLE_GAP_PHY_CODED,
    .phys.rx_phys             = BLE_GAP_PHY_2MBPS | BLE_GAP_PHY_1MBPS | BLE_GAP_PHY_CODED,
#else
    .phys.tx_phys             = BLE_GAP_PHY_1MBPS,//BLE_GAP_PHY_2MBPS | BLE_GAP_PHY_1MBPS,
    .phys.rx_phys             = BLE_GAP_PHY_1MBPS,//BLE_GAP_PHY_2MBPS | BLE_GAP_PHY_1MBPS,
#endif
};

// Scan parameters requested for scanning and connection.
static ble_gap_scan_params_t m_scan_param =
{
    .active         = 0x01,
    .interval       = SCAN_INTERVAL,
    .window         = SCAN_WINDOW,
    .use_whitelist  = 0x00,
    .adv_dir_report = 0x00,
    .timeout        = 0x0000, // No timeout.
};

// Connection parameters requested for connection.
static ble_gap_conn_params_t m_conn_param =
{
    .min_conn_interval = CONN_INTERVAL_MIN,   // Minimum connection interval.
    .max_conn_interval = CONN_INTERVAL_MAX,   // Maximum connection interval.
    .slave_latency     = SLAVE_LATENCY,       // Slave latency.
    .conn_sup_timeout  = CONN_SUP_TIMEOUT     // Supervisory timeout.
};

//deprecated
uint32_t adv_data_to_string(s_data_t *p_adv_data, char* p_str, uint8_t max_len);
uint32_t bd_addr_to_string(ble_gap_addr_t *p_addr, char* p_str, uint8_t max_len);

//application helpers
uint32_t add_node_to_report_payload(node_t *m_network, uint8_t* report_payload, uint16_t max_size);
void send_neighbors_report(void);
void cancel_ongoing_report(void);
static void send_pong(uart_pkt_t* p_packet);
void send_ping(void);
static void send_ready(void);

//uart communication helpers
extern void uart_util_rx_handler(uart_pkt_t* p_packet);
extern void uart_util_ack_tx_done(void);
extern void uart_util_ack_error(ack_wait_t* ack_wait_data);

//ToF
#ifdef ENABLE_TOF
static uint32_t calculate_ifs_offset(ble_gap_phys_t actual_phy);
static void tof_results_print(ifs_tof_t * p_ctx);
#endif

//BT neighbors list management
void on_scan_response(ble_gap_evt_adv_report_t const * p_adv_report);
void on_adv(ble_gap_evt_adv_report_t const * p_adv_report);
void network_maintainance_check(void);
uint32_t get_time_since_last_contact(node_t *p_node);
node_t* get_beacon_pos_by_bdaddr(const ble_gap_addr_t *target_addr);
node_t* get_network_pos_by_conn_handle(uint16_t target_conn_h);
uint8_t is_position_free(node_t *p_node);
node_t* get_first_free_network_pos(void);
static void remove_node_from_network(node_t* p_node);
static node_t* add_node_to_network(const ble_gap_evt_t *p_conn_evt, const ble_gap_evt_adv_report_t *p_adv_evt, node_type_t node_type);
static node_t* update_node(const ble_gap_evt_t *p_conn_evt, const ble_gap_evt_adv_report_t *p_adv_evt, node_t * p_node);
static void reset_node(node_t* p_node);
static void reset_network(void);

//application timers helpers/callbacks
void application_timers_start(void);
void start_periodic_report(uint32_t timeout_ms);
static void report_timeout_handler(void * p_context);
static void periodic_timer_timeout_handler(void * p_context);
static void led_timeout_handler(void * p_context);
static void lp_led_timeout_handler(void * p_context);
void blink_led(uint32_t led_idx, uint32_t blink_timeout);

//bt callbacks/helpers
static void ble_stack_init(void);
static void gap_params_init(void);
static void advertising_data_set(void);
void preferred_phy_set(ble_gap_phys_t * p_phy);
void conn_interval_set(uint16_t value);
static void ble_evt_handler(ble_evt_t const * p_ble_evt, void * p_context);
static void on_ble_gap_evt_disconnected(ble_gap_evt_t const * p_gap_evt);
static void on_ble_gap_evt_connected(ble_gap_evt_t const * p_gap_evt);
static void on_ble_gap_evt_adv_report(ble_gap_evt_t const * p_gap_evt);
static void scan_stop(void);
static void scan_start(void);
static void advertising_stop(void);
static void advertising_start(void);
static void set_scan_params(uint8_t scan_active, uint16_t scan_interval, uint16_t scan_window, uint16_t scan_timeout);
char const * phy_str(ble_gap_phys_t phys);
static uint32_t adv_report_parse(uint8_t type, data_t * p_advdata, data_t * p_typedata);
static uint32_t get_adv_name(ble_gap_evt_adv_report_t const * p_adv_report, data_t * dev_name);
static uint32_t get_adv_16bit_uuid(ble_gap_evt_adv_report_t const * p_adv_report, data_t * uuid_data);
static uint32_t get_adv_service_data(ble_gap_evt_adv_report_t const * p_adv_report, data_t * service_data, uint16_t target_uuid);
uint32_t get_eddystone_frame(ble_gap_evt_adv_report_t const * p_adv_report, data_t * dev_name);
uint32_t get_adv_manufacturer_specific_data(ble_gap_evt_adv_report_t const * p_adv_report, data_t * manufacturer_data, uint16_t target_manuf_uuid);
static bool is_known_name(ble_gap_evt_adv_report_t const * p_adv_report, char const * name_to_find);
bool is_16bit_uuid_included(ble_gap_evt_adv_report_t const * p_adv_report, uint16_t target_uuid);
static bool is_known_eddystone(ble_gap_evt_adv_report_t const * p_adv_report, uint8_t const * namespace_to_find);

//hardware init/callbacks/helpers
static void initialize_leds(void);
static void initialize_uart(void);
static void timer_init(void);
static void buttons_init(void);
static void buttons_enable(void);
static void button_evt_handler(uint8_t pin_no, uint8_t button_action);

//system
static void wait_for_event(void);

#ifdef ENABLE_TOF
void ifs_tof_evt_handler(ifs_tof_evt_t * p_evt);
static bool is_link_present(ble_gap_evt_adv_report_t const * p_adv_report);
#endif

uint32_t bd_addr_to_string(ble_gap_addr_t *p_addr, char* p_str, uint8_t max_len) {
//	return sprintf(p_str, "%02X:%02X:%02X:%02X:%02X:%02X", p_addr->addr[5], p_addr->addr[4], p_addr->addr[3], p_addr->addr[2], p_addr->addr[1],
//			p_addr->addr[0]);
	if (p_addr == NULL) {
		return 0;
	}
	uint8_t str_i = 0;
	for (int i = 0; i < 6 && str_i + 1 <= max_len; i++) {
		p_str[str_i++] = to_hex((p_addr->addr[5 - i] >> 4) & 0x0F);
		p_str[str_i++] = to_hex((p_addr->addr[5 - i]) & 0x0F);
	}
	return str_i;
}

uint32_t adv_data_to_string(s_data_t *p_adv_data, char* p_str, uint8_t max_len) {
	if (p_adv_data == NULL) {
		return 0;
	}
	uint8_t str_i = 0;
	for (int i = 0; i < p_adv_data->len && str_i + 1 <= max_len; i++) {
		p_str[str_i++] = to_hex((p_adv_data->p_data[i] >> 4) & 0x0F);
		p_str[str_i++] = to_hex((p_adv_data->p_data[i]) & 0x0F);
	}
	return str_i;
}

uint32_t add_node_to_report_payload(node_t *p_node, uint8_t* report_payload, uint16_t max_size) {
	if (p_node == NULL || report_payload == NULL || max_size < SINGLE_NODE_REPORT_SIZE) {
		return 0;
	}

	uint16_t idx = 0;

	//add ID
	memcpy(&report_payload[idx], p_node->instance_id, EDDYSTONE_INSTANCE_ID_LENGTH);
	idx += EDDYSTONE_INSTANCE_ID_LENGTH;

	//add last rssi
	report_payload[idx++] = (uint8_t) p_node->last_rssi;

	//add max rssi
	report_payload[idx++] = (uint8_t) p_node->max_rssi;

	//add packet counter
	report_payload[idx++] = p_node->beacon_msg_count;
	return idx;
}

void send_neighbors_report(void) {

	static uint16_t n = 0;
	bool pkt_full = false;
	uint8_t report_payload[UART_PKT_PAYLOAD_MAX_LEN_SYMB];
	uint16_t payload_free_from = 0;

	uart_pkt_t packet;

	if (report_procedure_running == 0) { //sanity check, if the report procedure was not active (maybe it was cancelled) reset n to zero
		n = 0;
	}

	report_procedure_running = 1;

	packet.payload.p_data = report_payload;
	if (n == 0) {	//if this is the first packet of the report the type will end with the '_start'
		packet.type = uart_resp_bt_neigh_rep_start;
	} else {		//otherwise it will be '_more'
		packet.type = uart_resp_bt_neigh_rep_more;
	}

	while (n < MAXIMUM_NETWORK_SIZE && pkt_full == false) {
		if (UART_PKT_PAYLOAD_MAX_LEN_SYMB - payload_free_from > SINGLE_NODE_REPORT_SIZE) {
			if (!is_position_free(&m_network[n])) {
				uint32_t occupied_size = add_node_to_report_payload(&m_network[n], &report_payload[payload_free_from], UART_PKT_PAYLOAD_MAX_LEN_SYMB - payload_free_from);
				payload_free_from += occupied_size;
			}
			n++;
		} else {
			pkt_full = true;
		}
	}

	packet.payload.data_len = payload_free_from;
	if (n >= MAXIMUM_NETWORK_SIZE) { //if it is the last packet for this report change the message type to uart_resp_bt_neigh_rep_end
		packet.type = uart_resp_bt_neigh_rep_end;
		n = 0;
		report_procedure_running = 0;
		payload_free_from = 0;
	}
	uart_util_send_pkt(&packet);

	if (n == 0) {	//this is executed after the transmission of uart_resp_bt_neigh_rep_end
		reset_network();
	}
}

void cancel_ongoing_report(void) {
	report_procedure_running = 0;
}

static void send_pong(uart_pkt_t* p_packet) {
	if (p_packet->payload.data_len) {
		uint8_t pong_payload[p_packet->payload.data_len];
		memcpy(pong_payload, p_packet->payload.p_data, p_packet->payload.data_len);

		uart_pkt_t pong_packet;
		pong_packet.payload.p_data = pong_payload;
		pong_packet.payload.data_len = p_packet->payload.data_len;
		pong_packet.type = uart_pong;

		uart_util_send_pkt(&pong_packet);
	} else {
		uart_pkt_t pong_packet;
		pong_packet.payload.p_data = NULL;
		pong_packet.payload.data_len = p_packet->payload.data_len;
		pong_packet.type = uart_pong;

		uart_util_send_pkt(&pong_packet);
	}
}

#define PING_PACKET_SIZE		10
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

static void send_ready(void) {

	char version_string[] = STRINGIFY(VERSION_STRING);

	uart_pkt_t pong_packet;
	pong_packet.payload.p_data = (uint8_t*)version_string;
	pong_packet.payload.data_len = strlen(version_string);
	pong_packet.type = uart_evt_ready;

	uart_util_send_pkt(&pong_packet);
}

void uart_util_rx_handler(uart_pkt_t* p_packet) {

	uart_pkt_type_t type = p_packet->type;
	uint8_t *p_payload_data = p_packet->payload.p_data;
	uint32_t ack_value = APP_ERROR_NOT_FOUND;

	if (report_procedure_running == 1 && type != uart_app_level_ack) { //during report procedures accept only ack packet
		ack_value = APP_ERROR_INVALID_STATE;
		uart_util_send_ack(p_packet, ack_value);
		return;
	}

	switch (type) {
	case uart_app_level_ack:
		if (p_packet->payload.data_len == 5) {
			if (p_payload_data[0] == APP_ACK_SUCCESS) { //if the app ack is positive go on with the report
				m_tx_error = false;
				if (report_procedure_running == 1) { //if it was ongoing
					send_neighbors_report();
				}
			} else {	//otherwise cancel this report
				blink_led(ERROR_LED, LED_BLINK_TIMEOUT_MS);
				cancel_ongoing_report();
			}
		}
		return; //this avoids sending ack for ack messages
		break;
	case uart_evt_ready:
		m_ready_received = true;
		blink_led(READY_LED, LED_BLINK_TIMEOUT_MS);
		ack_value = APP_ACK_SUCCESS;
		//still not implemented
		break;
	case uart_req_reset:
		sd_nvic_SystemReset();
		return;
		break;
	case uart_req_bt_state:
		ack_value = APP_ERROR_NOT_IMPLEMENTED;
		//still not implemented
		break;
	case uart_req_bt_scan_state:
		ack_value = APP_ERROR_NOT_IMPLEMENTED;
		//still not implemented
		break;
	case uart_req_bt_adv_state:
		ack_value = APP_ERROR_NOT_IMPLEMENTED;
		//still not implemented
		break;
	case uart_req_bt_scan_params:
		ack_value = APP_ERROR_NOT_IMPLEMENTED;
		//still not implemented
		break;
	case uart_req_bt_adv_params:
		ack_value = APP_ERROR_NOT_IMPLEMENTED;
		//still not implemented
		break;
	case uart_req_bt_neigh_rep:
		if (p_packet->payload.data_len == 4) {
            ack_value = APP_ACK_SUCCESS;
		    uart_util_send_ack(p_packet, ack_value);

			uint32_t timeout_ms = (p_payload_data[0] << 24) + (p_payload_data[1] << 16) + (p_payload_data[2] << 8) + (p_payload_data[3]);
			start_periodic_report(timeout_ms);
			blink_led(PROGRESS_LED, LED_BLINK_TIMEOUT_MS);
			return;
		}
		break;
	case uart_resp_bt_state:
	case uart_resp_bt_scan_state:
	case uart_resp_bt_adv_state:
	case uart_resp_bt_scan_params:
	case uart_resp_bt_adv_params:
	case uart_resp_bt_neigh_rep_start:
	case uart_resp_bt_neigh_rep_more:
	case uart_resp_bt_neigh_rep_end:
		ack_value = APP_ERROR_COMMAND_NOT_VALID;
		//never called on the BLE side of the uart
		break;
	case uart_set_bt_state:
		ack_value = APP_ERROR_COMMAND_NOT_VALID;
		//still not implemented
		break;
	case uart_set_bt_scan_state:
		if (p_packet->payload.data_len == 1) {
			if (p_packet->payload.p_data[0]) { //enable scanner
				scan_start();
			} else {	//disable scanner
				scan_stop();
			}
			//TODO: does it need to check something before sending ack?
			ack_value = APP_ACK_SUCCESS;
		}
		break;
	case uart_set_bt_adv_state:
		if (p_packet->payload.data_len == 1) {
			if (p_packet->payload.p_data[0]) { //enable advertiser
				advertising_start();
			} else {	//disable advertiser
				advertising_stop();
			}
			//TODO: does it need to check something before sending ack?
			ack_value = APP_ACK_SUCCESS;
		}
		break;
	case uart_set_bt_scan_params:
		if (p_packet->payload.data_len == 7) {
			uint8_t active = p_payload_data[0];
			uint16_t scan_interval = (p_payload_data[1] << 8) + p_payload_data[2];
			uint16_t scan_window = (p_payload_data[3] << 8) + p_payload_data[4];
			uint16_t timeout = (p_payload_data[5] << 8) + p_payload_data[6];
			set_scan_params(active, scan_interval, scan_window, timeout);
			//TODO: does it need to check something before sending ack?
			ack_value = APP_ACK_SUCCESS;
		}
		break;
	case uart_set_bt_adv_params:
		ack_value = APP_ERROR_NOT_IMPLEMENTED;
		//still not implemented
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

void uart_util_ack_tx_done(void) {
	m_tx_error = false;
	return;
}

void uart_util_ack_error(ack_wait_t* ack_wait_data) {
	blink_led(ERROR_LED, LED_BLINK_TIMEOUT_MS);
	m_tx_error = true;
}

#ifdef ENABLE_TOF
static uint32_t calculate_ifs_offset(ble_gap_phys_t actual_phy)
{ //check/compare results with matlab!
//#ifdef S140
	uint8_t data_rate_mbps;
	switch (actual_phy.rx_phys)
	{
		case BLE_GAP_PHY_1MBPS:
		data_rate_mbps = 1;
		return (uint32_t) (T_IFS_us + PREAMBLE_LEGNTH_us + ACCESS_ADDRESS_LENGTH_bit / data_rate_mbps + RX_CHAIN_DELAY_1MPHY_us)
		* IFS_TIMER_TICK_FREQUENCY_MHZ;

		case BLE_GAP_PHY_2MBPS:
		data_rate_mbps = 2;
		return (uint32_t) (T_IFS_us + PREAMBLE_LEGNTH_us + ACCESS_ADDRESS_LENGTH_bit / data_rate_mbps + RX_CHAIN_DELAY_2MPHY_us)
		* IFS_TIMER_TICK_FREQUENCY_MHZ;

		case BLE_GAP_PHY_CODED:
		return 0;
		break;
		default:
		data_rate_mbps = 1;
		return (uint32_t) (T_IFS_us + PREAMBLE_LEGNTH_us + ACCESS_ADDRESS_LENGTH_bit / data_rate_mbps + RX_CHAIN_DELAY_1MPHY_us)
		* IFS_TIMER_TICK_FREQUENCY_MHZ;
		break;
	}
//#else
//	return (uint32_t) (T_IFS_us + PREAMBLE_LEGNTH_us + ACCESS_ADDRESS_LENGTH_bit + RX_CHAIN_DELAY_1MPHY_us) * IFS_TIMER_TICK_FREQUENCY_MHZ;
//#endif
}

static void tof_results_print(ifs_tof_t * p_ctx)
{
//	if (m_board_role == BOARD_TESTER)
	{
		char c;
		if (m_test_params.continuous_test)
		{
			c = 's'; //when the test is continuous just print the summary
		}
		else
		{
			c = 'p';
		}

		if (c == 'p' || c == 's')
		{
			float ifs_duration_ticks_avg = 0;
			float rssi_avg = 0;

			static char addr_str[30]; //18 should be enough, but it mess up
			bd_addr_to_string(&p_ctx->bd_address, addr_str);

			for (uint32_t i = 0; i < TOF_SAMPLES_TO_ACQUIRE; i++)
			{
				if (c == 'p')
				{
					NRF_LOG_INFO("i = %u ifs_duration_ticks_avg = %u rssi = %d radio_freq = 0x%08x\n\r", i,
							p_ctx->buffer[i].ifs_duration_ticks, p_ctx->buffer[i].rssi,
							p_ctx->buffer[i].frequency_reg);
				}

				ifs_duration_ticks_avg += p_ctx->buffer[i].ifs_duration_ticks;
				rssi_avg += p_ctx->buffer[i].rssi;

				if (i % 25 == 0 && c == 'p')
				{
					NRF_LOG_FLUSH()
					;
				}
			}
			ifs_duration_ticks_avg = ifs_duration_ticks_avg / TOF_SAMPLES_TO_ACQUIRE;
			rssi_avg = rssi_avg / TOF_SAMPLES_TO_ACQUIRE;
			//NRF_LOG_INFO("Line with timestamp");
			NRF_LOG_INFO("bd_addr = %s ",addr_str);
#ifdef USE_NANOSECOND
			float ifs_duration_ns_comp_avg = (ifs_duration_ticks_avg - ifs_offset_ticks)/(((float)IFS_TIMER_TICK_FREQUENCY_MHZ) / 1000);
			NRF_LOG_RAW_INFO("ifs_duration_ns_avg = "NRF_LOG_FLOAT_MARKER" ns rssi_avg = "NRF_LOG_FLOAT_MARKER" dBm\n\r",NRF_LOG_FLOAT(ifs_duration_ns_comp_avg), NRF_LOG_FLOAT(rssi_avg));
#else
			NRF_LOG_RAW_INFO("Average values are ifs_duration_ticks_avg = "NRF_LOG_FLOAT_MARKER" rssi_avg = "NRF_LOG_FLOAT_MARKER"\n\r",
					NRF_LOG_FLOAT(ifs_duration_ticks_avg), NRF_LOG_FLOAT(rssi_avg));
#endif
			if (!m_test_params.continuous_test)
			{
				NRF_LOG_INFO("Total number of connection events %u, %u packets were not received",
						p_ctx->buffer[TOF_SAMPLES_TO_ACQUIRE- 1].connection_events_counter,
						p_ctx->buffer[TOF_SAMPLES_TO_ACQUIRE - 1].connection_events_counter - TOF_SAMPLES_TO_ACQUIRE);
			}
		}
	}
}

static bool is_adv_connectable(ble_gap_evt_adv_report_t const * p_adv_report)
{

	ret_code_t err_code;
	data_t flags;
	data_t adv_data;
	adv_data.p_data = (uint8_t *)p_adv_report->data;
	adv_data.data_len = p_adv_report->dlen;

	// Look for the short local name if the complete name was not found.
	err_code = adv_report_parse(BLE_GAP_AD_TYPE_FLAGS, &adv_data, &flags);

	if (err_code != NRF_SUCCESS)
	{
		return false;
	}
	else
	{
		return flags.p_data[0] && (BLE_GAP_ADV_FLAG_LE_GENERAL_DISC_MODE | BLE_GAP_ADV_FLAG_BR_EDR_NOT_SUPPORTED);
	}
}

static bool is_link_present(ble_gap_evt_adv_report_t const * p_adv_report)
{
	for(uint8_t n = 0; n < MAXIMUM_NETWORK_SIZE; n++)
	{
		if(m_network[n].conn_handle != BLE_CONN_HANDLE_INVALID)
		{
			if(memcmp(&p_adv_report->peer_addr, &m_network[n].bd_address, sizeof(ble_gap_addr_t) )== 0)
			{
				return true;
			}
		}
	}
	return false;
}

void tof_init()
{
	ifs_tof_init(m_ifs_tof);
	for (uint8_t i = 0; i < MAXIMUM_NUMBER_OF_CENTRAL_CONN; i++)
	{ //reset all instance of m_ifs_tof and register the same handler for each node
		ifs_tof_init_struct(&m_ifs_tof[i]);
	}
}
#endif


void on_scan_response(ble_gap_evt_adv_report_t const * p_adv_report) {
	node_t *node = get_beacon_pos_by_bdaddr(&p_adv_report->peer_addr);
	if (node != NULL) {
		update_node(NULL, p_adv_report, node);
	} else {
		//		if (is_known_name(p_adv_report, m_target_periph_name_alt)) {	//the name could be in the scan response. But not for CLIMB nodes
		//			add_node_to_network(NULL, p_adv_report, CLIMB_BEACON);
		//		}
	}
}

void on_adv(ble_gap_evt_adv_report_t const * p_adv_report) {
	node_type_t node_type = UNKNOWN_TYPE;
	if (is_known_name(p_adv_report, m_target_periph_name_alt)) {
		node_type = CLIMB_BEACON;
	} else if(is_known_name(p_adv_report, m_target_periph_name)){
		node_type = NORDIC_BEACON;
	}else{

		uint8_t valid_eddystone_namespace[] = VALID_EDDYSTONE_NAMESPACES;

		if (is_known_eddystone(p_adv_report, valid_eddystone_namespace)) {

			node_type = EDDYSTONE_BEACON;
		}
	}

	if (node_type == EDDYSTONE_BEACON || node_type == CLIMB_BEACON) {
		node_t *node = get_beacon_pos_by_bdaddr( &p_adv_report->peer_addr );
		if (node == NULL) {
			add_node_to_network(NULL, p_adv_report, node_type);
		} else {
			update_node(NULL, p_adv_report, node);
		}
	} else {
		return;
	}
}

void network_maintainance_check(void) {
	for (uint32_t n = 0; n < MAXIMUM_NETWORK_SIZE; n++) {
		if (m_network[n].first_contact_ticks != 0) { //exclude non valid nodes
			if (get_time_since_last_contact(&m_network[n]) > APP_TIMER_TICKS(beacon_timeout_ms)) {
				remove_node_from_network(&m_network[n]);
			}
		}
	}
}

uint32_t get_time_since_last_contact(node_t *p_node) {
	return app_timer_cnt_diff_compute(app_timer_cnt_get(), p_node->last_contact_ticks);
}

node_t* get_beacon_pos_by_bdaddr(const ble_gap_addr_t *target_addr) {
	for (uint8_t n = 0; n < MAXIMUM_NETWORK_SIZE; n++) {
		if (memcmp(&(m_network[n].bd_address), target_addr, sizeof(ble_gap_addr_t)) == 0) {
			return &m_network[n];
		}
	}
	return NULL;
}

node_t* get_network_pos_by_conn_handle(uint16_t target_conn_h) {
	for (uint8_t n = 0; n < MAXIMUM_NETWORK_SIZE; n++) {
		if (m_network[n].conn_handle == target_conn_h) {
			return &m_network[n];
		}
	}
	return NULL;
}

uint8_t is_position_free(node_t *p_node) {
	return p_node->conn_handle == BLE_CONN_HANDLE_INVALID && p_node->first_contact_ticks == 0;
}

node_t* get_first_free_network_pos(void) {
	for (uint8_t n = 0; n < MAXIMUM_NETWORK_SIZE; n++) {
		if (is_position_free(&m_network[n])) {
			return &m_network[n];
		}
	}
	return NULL;
}

static void remove_node_from_network(node_t* p_node) {
	if (p_node == NULL) {
		return;
	}

	reset_node(p_node);
	return;
}

static node_t* add_node_to_network(const ble_gap_evt_t *p_conn_evt, const ble_gap_evt_adv_report_t *p_adv_evt, node_type_t node_type) {
	node_t* p_node = get_first_free_network_pos();
	if (p_node == NULL) {
		return NULL;
	}

	if (p_adv_evt != NULL) {
		if (node_type == CLIMB_BEACON) {
			data_t manuf_data;

			if (get_adv_manufacturer_specific_data(p_adv_evt, &manuf_data, TI_MANUFACTURER_UUID) != NRF_SUCCESS) {
				return NULL; //manufacturer data not found
			}

			uint8_t climb_namespace[] = CLIMB_BEACON_NAMESPACE;
			if (manuf_data.data_len >= 5) { //climb manufacturer specific data length >= 7 (1B node id, 1B node state, 2B battery mV, 1B counter)
				memcpy(p_node->namespace_id, climb_namespace, EDDYSTONE_NAMESPACE_ID_LENGTH);
				memset(p_node->instance_id, 0x00, EDDYSTONE_INSTANCE_ID_LENGTH - 1); //zero padding the 5 unused bytes (CLIMB beacon id is one byte long)
				p_node->instance_id[EDDYSTONE_INSTANCE_ID_LENGTH-1] = manuf_data.p_data[0];
			}

		} else if (node_type == EDDYSTONE_BEACON) {
			data_t eddystone_frame;
			if (get_eddystone_frame(p_adv_evt, &eddystone_frame) != NRF_SUCCESS) {
				return NULL; //eddystone frame not found
			}

			if (eddystone_frame.p_data[0] == EDDYSTONE_UID_FRAME_TYPE) {
				memcpy(p_node->namespace_id, &eddystone_frame.p_data[2], EDDYSTONE_NAMESPACE_ID_LENGTH);
				memcpy(p_node->instance_id, &eddystone_frame.p_data[12], EDDYSTONE_INSTANCE_ID_LENGTH);
			} else if (eddystone_frame.p_data[0] == EDDYSTONE_URL_FRAME_TYPE) {
				return NULL; //don't know what to do with this kind of frame for now
			} else if (eddystone_frame.p_data[0] == EDDYSTONE_TLM_FRAME_TYPE) {
				return NULL; //don't know what to do with this kind of frame for now
			} else if (eddystone_frame.p_data[0] == EDDYSTONE_EID_FRAME_TYPE) {
				return NULL; //don't know what to do with this kind of frame for now
			} else {
				return NULL; //not valid frame type
			}
		} else if (node_type == NORDIC_BEACON) {
			return NULL;
		} else if (node_type == UNKNOWN_TYPE) {
			return NULL;
		} else {
			return NULL;
		}
	}

	if (p_adv_evt != NULL) {
		memcpy(&p_node->bd_address, &p_adv_evt->peer_addr, sizeof(ble_gap_addr_t));
	} else if (p_conn_evt != NULL) {
		memcpy(&p_node->bd_address, &p_conn_evt->params.connected.peer_addr, sizeof(ble_gap_addr_t));
	}
	p_node-> node_type = node_type;
	p_node->first_contact_ticks = app_timer_cnt_get();

	return update_node(p_conn_evt, p_adv_evt, p_node);
}

static node_t* update_node(const ble_gap_evt_t *p_conn_evt, const ble_gap_evt_adv_report_t *p_adv_evt, node_t * p_node) {
	if (p_node == NULL) {
		return NULL;
	}

	if (p_conn_evt != NULL) {
		p_node->conn_handle = p_conn_evt->conn_handle;
		p_node->local_role = p_conn_evt->params.connected.role;
	} else if (p_adv_evt != NULL) {
		p_node->last_rssi = p_adv_evt->rssi;
		if (p_node->last_rssi > p_node->max_rssi) {
			p_node->max_rssi = p_node->last_rssi;
		}
		if (p_adv_evt->scan_rsp) {
			memcpy(&p_node->scan_data.p_data, &p_adv_evt->data, p_adv_evt->dlen);
			p_node->scan_data.len = p_adv_evt->dlen;
		} else {
			p_node->beacon_msg_count++;
			memcpy(&p_node->adv_data.p_data, &p_adv_evt->data, p_adv_evt->dlen);
			p_node->adv_data.len = p_adv_evt->dlen;
		}
	}

	p_node->last_contact_ticks = app_timer_cnt_get();

	return p_node;
}

static void reset_node(node_t* p_node) {
	memset(&p_node->bd_address, 0x00, sizeof(ble_gap_addr_t));
	p_node->conn_handle = BLE_CONN_HANDLE_INVALID;
	p_node->local_role = BLE_GAP_ROLE_INVALID;
	p_node->max_rssi = INT8_MIN;
	p_node->first_contact_ticks = 0;
}

static void reset_network(void) {
	memset(m_network, 0x00, MAXIMUM_NETWORK_SIZE * sizeof(node_t));
	for (uint8_t n = 0; n < MAXIMUM_NETWORK_SIZE; n++) {
		reset_node(&m_network[n]);
	}
}

/**@brief Function for starting application timers.
 */
void application_timers_start(void) {
	ret_code_t err_code;

	// Start application timers.
	err_code = app_timer_start(m_periodic_timer_id, PERIODIC_TIMER_INTERVAL, NULL);
	APP_ERROR_CHECK(err_code);

	// Start application timers.
	err_code = app_timer_start(m_lp_led_timer_id, LP_LED_INTERVAL, NULL);
	APP_ERROR_CHECK(err_code);
}

void start_periodic_report(uint32_t timeout_ms) {
	if (timeout_ms != 0 && timeout_ms < MIN_REPORT_TIMEOUT_MS) {
		return;
	}

	if (timeout_ms != 0) {
		ret_code_t err_code;
		if (m_periodic_report_timeout) { //if the report was already active, stop the timer to permit the timeout update
			err_code = app_timer_stop(m_report_timer_id);
			APP_ERROR_CHECK(err_code);
		}
		err_code = app_timer_start(m_report_timer_id, APP_TIMER_TICKS(timeout_ms), NULL);  //in any case if timeout_ms != 0 enable the report
		APP_ERROR_CHECK(err_code);
		m_periodic_report_timeout = timeout_ms;
		beacon_timeout_ms = timeout_ms;
	} else {
		if (m_periodic_report_timeout) { //if this command is sent with timeout_ms == 0 disable the report. If the report wasn't enabled, send a single report, if it was enabled stop everithing
			ret_code_t err_code;
			err_code = app_timer_stop(m_report_timer_id);
			APP_ERROR_CHECK(err_code);
			m_periodic_report_timeout = 0;
		} else {
			send_neighbors_report();
		}
	}
}

static void report_timeout_handler(void * p_context) {
	UNUSED_PARAMETER(p_context);

	send_neighbors_report();
}

static void periodic_timer_timeout_handler(void * p_context) {
	UNUSED_PARAMETER(p_context);
}

static void led_timeout_handler(void * p_context) {
	uint32_t * p_blinking_led_bit_map = (uint32_t*)p_context;
	for(uint8_t led_idx = 0; led_idx < LEDS_NUMBER; led_idx++){
		if((*p_blinking_led_bit_map >> led_idx) & 0b1){
			bsp_board_led_off(led_idx);
		}
	}
	*p_blinking_led_bit_map = 0;
}

static void lp_led_timeout_handler(void * p_context) {
	uint32_t led_to_blink = 0;
	if(m_scan_active || m_advertising_active) led_to_blink |= SCAN_ADV_LED;
	if(m_ready_received)  led_to_blink |= READY_LED;
	if(m_tx_error)  led_to_blink |= ERROR_LED;
	if(m_periodic_report_timeout) led_to_blink |= PROGRESS_LED;

	blink_led(led_to_blink, LED_BLINK_TIMEOUT_MS);
}

void blink_led(uint32_t led, uint32_t blink_timeout_ms) {
	static uint32_t blinking_led_bit_map = 0;
	blinking_led_bit_map |= led;
	for(uint8_t led_idx = 0; led_idx < LEDS_NUMBER; led_idx++){
		if((blinking_led_bit_map >> led_idx) & 0b1){
			bsp_board_led_on(led_idx);
		}
	}
	app_timer_start(m_led_blink_timer_id, APP_TIMER_TICKS(blink_timeout_ms), &blinking_led_bit_map);
}

/**@brief Function for initializing the BLE stack.
 *
 * @details Initializes the SoftDevice and the BLE event interrupt.
 */
static void ble_stack_init(void) {
	ret_code_t err_code;

	err_code = nrf_sdh_enable_request();
	APP_ERROR_CHECK(err_code);

	// Configure the BLE stack using the default settings.
	// Fetch the start address of the application RAM.
	uint32_t ram_start = 0;
	err_code = nrf_sdh_ble_default_cfg_set(APP_BLE_CONN_CFG_TAG, &ram_start);
	APP_ERROR_CHECK(err_code);

	// Enable BLE stack.
	err_code = nrf_sdh_ble_enable(&ram_start);
	APP_ERROR_CHECK(err_code);

	// Register a handler for BLE events.
	NRF_SDH_BLE_OBSERVER(m_ble_observer, APP_BLE_OBSERVER_PRIO, ble_evt_handler, NULL);

	set_scan_params(DEFAULT_ACTIVE_SCAN, DEFAULT_SCAN_INTERVAL, DEFAULT_SCAN_WINDOW, DEFAULT_TIMEOUT);
}

/**@brief Function for initializing GAP parameters.
 *
 * @details This function sets up all the necessary GAP (Generic Access Profile) parameters of the
 *          device including the device name and the preferred connection parameters.
 */
static void gap_params_init(void) {
	ret_code_t err_code;
	ble_gap_conn_sec_mode_t sec_mode;

	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);

	err_code = sd_ble_gap_device_name_set(&sec_mode, (uint8_t const *) DEVICE_NAME, strlen(DEVICE_NAME));
	APP_ERROR_CHECK(err_code);

	err_code = sd_ble_gap_ppcp_set(&m_conn_param);
	APP_ERROR_CHECK(err_code);
}

/**@brief Function for setting up advertising data. */
static void advertising_data_set(void) {
	ble_advdata_t const adv_data = { .name_type = BLE_ADVDATA_FULL_NAME, .flags = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE, .include_appearance =
	false, };

	ret_code_t err_code = ble_advdata_set(&adv_data, NULL);
	APP_ERROR_CHECK(err_code);
}

void preferred_phy_set(ble_gap_phys_t * p_phy) {
#if defined(S140)
	ble_opt_t opts;
	memset(&opts, 0x00, sizeof(ble_opt_t));
	memcpy(&opts.gap_opt.preferred_phys, p_phy, sizeof(ble_gap_phys_t));
	ret_code_t err_code = sd_ble_opt_set(BLE_GAP_OPT_PREFERRED_PHYS_SET, &opts);
	APP_ERROR_CHECK(err_code);
#endif
	memcpy(&m_test_params.phys, p_phy, sizeof(ble_gap_phys_t));
}

void conn_interval_set(uint16_t value) {
	m_test_params.conn_interval = value;
}

/**@brief Function for handling BLE Stack events.
 *
 * @param[in] p_ble_evt  Bluetooth stack event.
 */
static void ble_evt_handler(ble_evt_t const * p_ble_evt, void * p_context) {
	uint32_t err_code;
	ble_gap_evt_t const * p_gap_evt = &p_ble_evt->evt.gap_evt;

	switch (p_ble_evt->header.evt_id) {
	case BLE_GAP_EVT_ADV_REPORT:
		on_ble_gap_evt_adv_report(p_gap_evt);
		break;

	case BLE_GAP_EVT_CONNECTED:
		on_ble_gap_evt_connected(p_gap_evt);
		break;

	case BLE_GAP_EVT_DISCONNECTED:
		on_ble_gap_evt_disconnected(p_gap_evt);
		break;

	case BLE_GAP_EVT_CONN_PARAM_UPDATE: {
		m_conn_interval_configured = true;
//            NRF_LOG_INFO("Connection interval updated: 0x%x, 0x%x.\n\r",
//                p_gap_evt->params.conn_param_update.conn_params.min_conn_interval,
//                p_gap_evt->params.conn_param_update.conn_params.max_conn_interval);
	}
		break;

	case BLE_GAP_EVT_CONN_PARAM_UPDATE_REQUEST: {
		// Accept parameters requested by the peer.
		ble_gap_conn_params_t params;
		params = p_gap_evt->params.conn_param_update_request.conn_params;
		err_code = sd_ble_gap_conn_param_update(p_gap_evt->conn_handle, &params);
		APP_ERROR_CHECK(err_code);

//            NRF_LOG_INFO("Connection interval updated (upon request): 0x%x, 0x%x.\n\r",
//                p_gap_evt->params.conn_param_update_request.conn_params.min_conn_interval,
//                p_gap_evt->params.conn_param_update_request.conn_params.max_conn_interval);
	}
		break;

	case BLE_GATTS_EVT_SYS_ATTR_MISSING: {
		err_code = sd_ble_gatts_sys_attr_set(p_gap_evt->conn_handle, NULL, 0, 0);
		APP_ERROR_CHECK(err_code);
	}
		break;

	case BLE_GATTC_EVT_TIMEOUT: // Fallthrough.
	case BLE_GATTS_EVT_TIMEOUT: {
		//NRF_LOG_DEBUG("GATT timeout, disconnecting.\n\r");
		err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gap_evt.conn_handle,
		BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
		APP_ERROR_CHECK(err_code);
	}
		break;

	case BLE_EVT_USER_MEM_REQUEST: {
		err_code = sd_ble_user_mem_reply(p_ble_evt->evt.common_evt.conn_handle, NULL);
		APP_ERROR_CHECK(err_code);
	}
		break;
#ifdef ENABLE_PHY_UPDATE
		case BLE_GAP_EVT_PHY_UPDATE:
		{
			ble_gap_evt_phy_update_t const * p_phy_evt = &p_ble_evt->evt.gap_evt.params.phy_update;

			if (p_phy_evt->status == BLE_HCI_STATUS_CODE_LMP_ERROR_TRANSACTION_COLLISION)
			{
				// Ignore LL collisions.
				NRF_LOG_DEBUG("LL transaction collision during PHY update.\n\r");
				break;
			}

			m_phy_updated = true;

			ble_gap_phys_t phys =
			{	0};
			phys.tx_phys = p_phy_evt->tx_phy;
			phys.rx_phys = p_phy_evt->rx_phy;

			ifs_offset_ticks = calculate_ifs_offset(phys);

			NRF_LOG_INFO("PHY update %s. PHY set to %s.\n\r",
					(p_phy_evt->status == BLE_HCI_STATUS_CODE_SUCCESS) ?
					"accepted" : "rejected",
					phy_str(phys));

		}break;
#endif
#if defined(S132)
	case BLE_GAP_EVT_PHY_UPDATE_REQUEST: {
		err_code = sd_ble_gap_phy_update(p_gap_evt->conn_handle, &m_test_params.phys);
		APP_ERROR_CHECK(err_code);
	}
		break;
#endif

	default:
		// No implementation needed.
		break;
	}
}
#ifdef ENABLE_TOF
static void on_ifs_tof_buffer_full(ifs_tof_t * p_ctx)
{
	if (!m_test_params.continuous_test)
	{
		test_terminate();
	}
	tof_results_print(p_ctx);
}

/**@brief Evt dispatcher for ifs_tof module*/
void ifs_tof_evt_handler(ifs_tof_evt_t * p_evt)
{

	switch (p_evt->evt_type)
	{
		case IFS_TOF_EVT_BUFFER_FULL:
		{
			on_ifs_tof_buffer_full(p_evt->p_ctx);
		}
		break;
		default:
		{
			//no implementation needed
		}
		break;
	}
}
#endif

/**@brief Function for handling BLE_GAP_EVT_DISCONNECTED events.
 * Unset the connection handle and terminate the test.
 */
static void on_ble_gap_evt_disconnected(ble_gap_evt_t const * p_gap_evt) {
#ifdef ENABLE_TOF
	ifs_tof_unregister_conn_handle(p_gap_evt->conn_handle);

	//NRF_LOG_DEBUG("Disconnected: reason 0x%x.\n\r", p_gap_evt->params.disconnected.reason);

	node_t * p_node = get_network_pos_by_conn_handle(p_gap_evt->conn_handle);
	if (p_node->local_role == BLE_GAP_ROLE_CENTRAL)
	{
		m_no_of_central_links--;
		if (m_run_test)
		{
			NRF_LOG_WARNING("GAP disconnection from Central role while test was running.\n\r")
		}
	}
	else
	{
		m_no_of_peripheral_links--;
		if (m_run_test)
		{
			NRF_LOG_WARNING("GAP disconnection from Peripheral role while test was running.\n\r")
		}
	}
	if(m_board_role == BOARD_DUMMY)
	{
		advertising_start();
	}
	scan_start();

	if(m_no_of_central_links == 0)
	{
		//bsp_board_leds_off();
		//	test_terminate();
	}

	remove_node_from_network(p_gap_evt);
#endif
}

/**@brief Function for handling BLE_GAP_EVT_CONNECTED events.
 * Save the connection handle and GAP role, then discover the peer DB.
 */
static void on_ble_gap_evt_connected(ble_gap_evt_t const * p_gap_evt) {
#ifdef ENABLE_TOF
#warning the node_t has changed. The logic of connecting/disconnecting may be broken
	ret_code_t err_code;
//    m_conn_handle = p_gap_evt->conn_handle;
//    m_gap_role    = p_gap_evt->params.connected.role;
	node_t* p_node = add_node_to_network(p_gap_evt, NULL);
	if(p_node == NULL)
	{
		return;
	}
	bsp_board_leds_off();

	if (p_node->local_role == BLE_GAP_ROLE_PERIPH)
	{
		NRF_LOG_INFO("Connected as a peripheral.\n\r");
		m_no_of_peripheral_links++;
		if (m_no_of_peripheral_links < MAXIMUM_NUMBER_OF_PERIPH_CONN)
		{
			m_advertising_active = false; //workaround needed
			advertising_start();
		}
		else
		{
			advertising_stop();
		}
	}
	else if (p_node->local_role == BLE_GAP_ROLE_CENTRAL)
	{
		NRF_LOG_INFO("Connected as a central.\n\r");

		m_no_of_central_links++;
		ifs_tof_register_conn_handle(p_gap_evt, ifs_tof_evt_handler);
		if(m_board_role == BOARD_DUMMY)
		{
			advertising_start();
		}
		if (m_no_of_central_links < MAXIMUM_NUMBER_OF_CENTRAL_CONN)
		{
			scan_start();
		}
		else
		{
			scan_stop();
		}

		if (m_test_params.conn_interval != CONN_INTERVAL_DEFAULT)
		{
			NRF_LOG_DEBUG("Updating connection parameters..\n\r");
			m_conn_param.min_conn_interval = m_test_params.conn_interval;
			m_conn_param.max_conn_interval = m_test_params.conn_interval;
			err_code = sd_ble_gap_conn_param_update(p_node->local_role, &m_conn_param);

			if (err_code != NRF_SUCCESS)
			{
				NRF_LOG_ERROR("sd_ble_gap_conn_param_update() failed: 0x%x.\n\r", err_code);
			}
		}
		else
		{
			m_conn_interval_configured = true;
		}

#ifdef ENABLE_PHY_UPDATE
		if (m_gap_role == BLE_GAP_ROLE_PERIPH)
		{
#if defined(S140)
			err_code = sd_ble_gap_phy_request(p_gap_evt->conn_handle, &m_test_params.phys);
			APP_ERROR_CHECK(err_code);
#else
			err_code = sd_ble_gap_phy_update(p_gap_evt->conn_handle, &m_test_params.phys);
			APP_ERROR_CHECK(err_code);
#endif
		}
#endif
	}

	NRF_LOG_DEBUG("Connection interval: "NRF_LOG_FLOAT_MARKER" ms, Slave latency: %d, Supervision timeout %d ms.\n\r", NRF_LOG_FLOAT(1.25 * p_gap_evt->params.connected.conn_params.max_conn_interval), p_gap_evt->params.connected.conn_params.slave_latency, p_gap_evt->params.connected.conn_params.conn_sup_timeout * 10 );

#endif
}

/**@brief Function for handling BLE_GAP_ADV_REPORT events.
 * Search for a peer with matching device name.
 * If found, stop advertising and send a connection request to the peer.
 */
static void on_ble_gap_evt_adv_report(ble_gap_evt_t const * p_gap_evt) {

	if(report_procedure_running){ //do not update the network during report, just for safety
		return;
	}

	if (p_gap_evt->params.adv_report.scan_rsp) {	//if it is a scan response it won't contain eddystone frame. It may contain the name. And it may be valid if the node is already in the list
		return on_scan_response(&p_gap_evt->params.adv_report);
	} else {
		return on_adv(&p_gap_evt->params.adv_report);
	}
#ifdef ENABLE_TOF

	if (!is_adv_connectable(&p_gap_evt->params.adv_report))
	{
		return;
	}

	if(is_link_present(&p_gap_evt->params.adv_report))
	{
		return;
	}


	if (allow_new_connections && m_no_of_central_links < MAXIMUM_NUMBER_OF_CENTRAL_CONN)
	{
		//m_no_of_central_links = false;
		NRF_LOG_INFO("Device \"%s\" found, sending a connection request.\n\r", (uint32_t ) m_target_periph_name);

		scan_stop();
		advertising_stop();
		// Stop advertising.
		//(void) sd_ble_gap_adv_stop();

		// Initiate connection.
		m_conn_param.min_conn_interval = CONN_INTERVAL_DEFAULT;
		m_conn_param.max_conn_interval = CONN_INTERVAL_DEFAULT;

		ret_code_t err_code;
		err_code = sd_ble_gap_connect(&p_gap_evt->params.adv_report.peer_addr, &m_scan_param, &m_conn_param,
				APP_BLE_CONN_CFG_TAG);

		if (err_code != NRF_SUCCESS)
		{
			NRF_LOG_ERROR("sd_ble_gap_connect() failed: 0x%x.\n\r", err_code);
		}
	}
#endif
}

static void scan_stop(void) {
	if (m_scan_active) {
//		NRF_LOG_INFO("Stopping scan.\n\r");

		blink_led(SCAN_ADV_LED, LED_BLINK_TIMEOUT_MS);
		m_scan_active = false;
		ret_code_t err_code = sd_ble_gap_scan_stop();
		APP_ERROR_CHECK(err_code);
	}
}

/**@brief Function to start scanning. */
static void scan_start(void) {
	if (!m_scan_active) {
//        NRF_LOG_INFO("Starting scan.\n\r");
		reset_network();
		blink_led(SCAN_ADV_LED, LED_BLINK_TIMEOUT_MS);
		m_scan_active = true;
		ret_code_t err_code = sd_ble_gap_scan_start(&m_scan_param);
		APP_ERROR_CHECK(err_code);
	}
}

/**@brief Function for starting advertising. */
static void advertising_stop(void) {
	if (m_advertising_active) {
//		NRF_LOG_INFO("Stopping advertising.\n\r");

		blink_led(SCAN_ADV_LED, LED_BLINK_TIMEOUT_MS);
		m_advertising_active = false;
		ret_code_t err_code = sd_ble_gap_adv_stop();
		APP_ERROR_CHECK(err_code);
	}
}

/**@brief Function for starting advertising. */
static void advertising_start(void) {
	if (!m_advertising_active) {
		ble_gap_adv_params_t const adv_params = { .type = BLE_GAP_ADV_TYPE_ADV_IND, .p_peer_addr = NULL, .fp = BLE_GAP_ADV_FP_ANY, .interval =
		ADV_INTERVAL, .timeout = 0, };

//		NRF_LOG_INFO("Starting advertising.\n\r");

		blink_led(SCAN_ADV_LED, LED_BLINK_TIMEOUT_MS);
		m_advertising_active = true;
		ret_code_t err_code = sd_ble_gap_adv_start(&adv_params, APP_BLE_CONN_CFG_TAG);
		APP_ERROR_CHECK(err_code);
	}
}

static void set_scan_params(uint8_t scan_active, uint16_t scan_interval, uint16_t scan_window, uint16_t scan_timeout) {
	if (m_scan_active) {
		ret_code_t err_code = sd_ble_gap_scan_stop();
		APP_ERROR_CHECK(err_code);
	}

	m_scan_param.active = scan_active;
	m_scan_param.interval = scan_interval;
	m_scan_param.window = scan_window;
	m_scan_param.timeout = scan_timeout;

	if (m_scan_active) {
		ret_code_t err_code = sd_ble_gap_scan_start(&m_scan_param);
		APP_ERROR_CHECK(err_code);
	}
}

char const * phy_str(ble_gap_phys_t phys) {
	static char const * str[] = { "1 Mbps", "2 Mbps", "Coded", "Unknown" };

	switch (phys.tx_phys) {
	case BLE_GAP_PHY_1MBPS:
		return str[0];

	case BLE_GAP_PHY_2MBPS:
	case BLE_GAP_PHY_2MBPS | BLE_GAP_PHY_1MBPS:
	case BLE_GAP_PHY_2MBPS | BLE_GAP_PHY_1MBPS | BLE_GAP_PHY_CODED:
		return str[1];

	case BLE_GAP_PHY_CODED:
		return str[2];
	default:
		return str[3];
	}
}

/**@brief Parses advertisement data, providing length and location of the field in case
 *        matching data is found.
 *
 * @param[in]  Type of data to be looked for in advertisement data.
 * @param[in]  Advertisement report length and pointer to report.
 * @param[out] If data type requested is found in the data report, type data length andwhile(1){

 }
 *             pointer to data will be populated here.
 *
 * @retval NRF_SUCCESS if the data type is found in the report.
 * @retval NRF_ERROR_NOT_FOUND if the data type could not be found.
 */
static uint32_t adv_report_parse(uint8_t type, data_t * p_advdata, data_t * p_typedata) {
	uint32_t index = 0;
	uint8_t * p_data;

	p_data = p_advdata->p_data;

	while (index < p_advdata->data_len) {
		uint8_t field_length = p_data[index];
		uint8_t field_type = p_data[index + 1];

		if (field_type == type) {
			p_typedata->p_data = &p_data[index + 2];
			p_typedata->data_len = field_length - 1;
			return NRF_SUCCESS;
		}
		index += field_length + 1;
	}
	return NRF_ERROR_NOT_FOUND;
}


/**@brief Function for searching the name in the advertisement packets.
 *
 * @details Use this function to parse received advertising data and to find the name
 * in them either as 'complete_local_name' or as 'short_local_name'.
 *
 * @param[in]   p_adv_report   advertising data to parse.
 * @param[in]   dev_name       output data structure
 * @return   NRF_SUCCESS if the name was found, NRF_ERROR_NOT_FOUND otherwise.
 */
static uint32_t get_adv_name(ble_gap_evt_adv_report_t const * p_adv_report, data_t * dev_name) {

	ret_code_t err_code;
	data_t adv_data;

	// Initialize advertisement report for parsing.
	adv_data.p_data = (uint8_t *) p_adv_report->data;
	adv_data.data_len = p_adv_report->dlen;

	// Search for matching advertising names.
	err_code = adv_report_parse(BLE_GAP_AD_TYPE_COMPLETE_LOCAL_NAME, &adv_data, dev_name);

	if (err_code != NRF_SUCCESS) {
		// Look for the short local name if the complete name was not found.
		err_code = adv_report_parse(BLE_GAP_AD_TYPE_SHORT_LOCAL_NAME, &adv_data, dev_name);

	}

	return err_code;
}

//finds the first 16bit uuid included in the adv data
static uint32_t get_adv_16bit_uuid(ble_gap_evt_adv_report_t const * p_adv_report, data_t * uuid_data) {

	ret_code_t err_code;
	data_t adv_data;

	// Initialize advertisement report for parsing.
	adv_data.p_data = (uint8_t *) p_adv_report->data;
	adv_data.data_len = p_adv_report->dlen;

	err_code = adv_report_parse(BLE_GAP_AD_TYPE_16BIT_SERVICE_UUID_COMPLETE, &adv_data, uuid_data);

	return err_code;
}

static uint32_t get_adv_service_data(ble_gap_evt_adv_report_t const * p_adv_report, data_t * service_data, uint16_t target_uuid) {

	ret_code_t err_code;
	data_t adv_data;

	// Initialize advertisement report for parsing.
	adv_data.p_data = (uint8_t *) p_adv_report->data;
	adv_data.data_len = p_adv_report->dlen;

	err_code = adv_report_parse(BLE_GAP_AD_TYPE_SERVICE_DATA, &adv_data, service_data);
	if (err_code == NRF_SUCCESS) {
		if(service_data->data_len > 1){
			uint16_t uuid = (service_data->p_data[1] << 8) + service_data->p_data[0];
			if (uuid == target_uuid) {
				service_data->p_data = &service_data->p_data[2];
				service_data->data_len = service_data->data_len - 2;
			}else{	//service data found, but with wrong uuid
				err_code = NRF_ERROR_NOT_FOUND;
			}
		}else{	//service data found but with zero len. Probably the packet is badly formatted by the beacon
			err_code = NRF_ERROR_INVALID_LENGTH;
		}
	}

	return err_code;
}

uint32_t get_eddystone_frame(ble_gap_evt_adv_report_t const * p_adv_report, data_t * eddystone_frame){

	ret_code_t err_code;

	err_code = get_adv_service_data(p_adv_report, eddystone_frame, EDDYSTONE_SERVICE_UUID_16BIT);
	if (err_code == NRF_SUCCESS) {
		if (eddystone_frame->data_len == 0) {
			err_code = NRF_ERROR_INVALID_LENGTH; //the length is not compatible with eddystone
		}
	} else {
		err_code = NRF_ERROR_NOT_FOUND; //service data not found
	}

	return err_code;
}

uint32_t get_adv_manufacturer_specific_data(ble_gap_evt_adv_report_t const * p_adv_report, data_t * manufacturer_data, uint16_t target_manuf_uuid){

	ret_code_t err_code;
	data_t adv_data;

	// Initialize advertisement report for parsing.
	adv_data.p_data = (uint8_t *) p_adv_report->data;
	adv_data.data_len = p_adv_report->dlen;

	err_code = adv_report_parse(BLE_GAP_AD_TYPE_MANUFACTURER_SPECIFIC_DATA, &adv_data, manufacturer_data);
	if (err_code == NRF_SUCCESS) {
		if(manufacturer_data->data_len > 1){
			uint16_t uuid = (manufacturer_data->p_data[1] << 8) + manufacturer_data->p_data[0];
			if (uuid == target_manuf_uuid) {
				manufacturer_data->p_data = &manufacturer_data->p_data[2];
				manufacturer_data->data_len = manufacturer_data->data_len - 2;
			}else{	//service data found, but with wrong uuid
				err_code = NRF_ERROR_NOT_FOUND;
			}
		}else{	//service data found but with zero len. Probably the packet is badly formatted by the beacon
			err_code = NRF_ERROR_INVALID_LENGTH;
		}
	}

	return err_code;
}

/**@brief Function for searching a given name in the advertisement packets.
 *
 * @details Use this function to parse received advertising data and to find a given
 * name in them either as 'complete_local_name' or as 'short_local_name'.
 *
 * @param[in]   p_adv_report   advertising data to parse.
 * @param[in]   name_to_find   name to search.
 * @return   true if the given name was found, false otherwise.
 */
static bool is_known_name(ble_gap_evt_adv_report_t const * p_adv_report, char const * name_to_find) {

	ret_code_t err_code;
	data_t dev_name;
	bool found = false;

	err_code = get_adv_name(p_adv_report, &dev_name);

	if(err_code == NRF_SUCCESS){
		if((strlen(name_to_find) == dev_name.data_len) && (memcmp(name_to_find, dev_name.p_data, dev_name.data_len) == 0)){
			found = true;
		}
	}

	return found;
}

bool is_16bit_uuid_included(ble_gap_evt_adv_report_t const * p_adv_report, uint16_t target_uuid){
	ret_code_t err_code;
	bool found = false;
	data_t uuid_data;

	err_code = get_adv_16bit_uuid(p_adv_report, &uuid_data);
	if (err_code == NRF_SUCCESS) {
		if (uuid_data.data_len == 2) {
			uint16_t uuid = (uuid_data.p_data[1] << 8) + uuid_data.p_data[0];
			if (uuid == EDDYSTONE_SERVICE_UUID_16BIT) { //the uuid has been found and it equals the eddystone uuid
				found = true;
			} else { 	//an uuid has been found, but it doesn't mathc the eddystone uuid
				found = false;
			}
		} else {	//the uuid data found is not long 2 (this should never happen)
			found = false;
		}
	} else {	//no 16bit uuid found in the adv packet
		found = false;
	}

	return found;
}

static bool is_known_eddystone(ble_gap_evt_adv_report_t const * p_adv_report, uint8_t const * namespace_to_find) {

	ret_code_t err_code;

#if 1
	/* In the next two parts first we check if the eddystone uuid is broadcasted in the packet, then we check
	 * if the eddystone frame is present in the packet. Probably checking just the second condition is enough.
	 * For skipping the first check and speed up the process change to zero the #if statement
	 */

	if(!is_16bit_uuid_included(p_adv_report, EDDYSTONE_SERVICE_UUID_16BIT)){
		return false; // uuid not found
	}
#endif

	data_t eddystone_frame;
	err_code = get_eddystone_frame(p_adv_report, &eddystone_frame);
	if (err_code != NRF_SUCCESS) {
		return false; //eddystone frame not found
	}

	if (eddystone_frame.p_data[0] == EDDYSTONE_UID_FRAME_TYPE) {
		if (memcmp(&eddystone_frame.p_data[2], namespace_to_find, EDDYSTONE_NAMESPACE_ID_LENGTH) == 0) {
			return true;
		} else {	//the eddystone namespace do not match with namespace_to_find
			return false;
		}
	} else if (eddystone_frame.p_data[0] == EDDYSTONE_URL_FRAME_TYPE) {
		return false; //don't know what to do with this kind of frame for now
	} else if (eddystone_frame.p_data[0] == EDDYSTONE_TLM_FRAME_TYPE) {
		return false; //don't know what to do with this kind of frame for now
	} else if (eddystone_frame.p_data[0] == EDDYSTONE_EID_FRAME_TYPE) {
		return false; //don't know what to do with this kind of frame for now
	} else {
		return false; //not valid frame type
	}

}

void initialize_leds(void) {
	bsp_board_leds_init();
	bsp_board_leds_off();
}

void initialize_uart(void) {
	uart_util_initialize();
}

/**@brief Function for the Timer initialization.
 *
 * @details Initializes the timer module. This creates and starts application timers.
 */
static void timer_init(void) {
	ret_code_t err_code = app_timer_init();
	APP_ERROR_CHECK(err_code);

	// Create timers.
	err_code = app_timer_create(&m_periodic_timer_id, APP_TIMER_MODE_REPEATED, periodic_timer_timeout_handler);
	APP_ERROR_CHECK(err_code);

	err_code = app_timer_create(&m_report_timer_id, APP_TIMER_MODE_REPEATED, report_timeout_handler);
	APP_ERROR_CHECK(err_code);

	err_code = app_timer_create(&m_led_blink_timer_id, APP_TIMER_MODE_SINGLE_SHOT, led_timeout_handler);
	APP_ERROR_CHECK(err_code);

	err_code = app_timer_create(&m_lp_led_timer_id, APP_TIMER_MODE_REPEATED, lp_led_timeout_handler);
	APP_ERROR_CHECK(err_code);


}

/**@brief Function for initializing the button library.
 */
static void buttons_init(void) {
	// The array must be static because a pointer to it will be saved in the button library.
	static app_button_cfg_t buttons[] = {
											{
											TOGGLE_SCAN_BUTTON,
											false,
											BUTTON_PULL,
											button_evt_handler
											},
											{
											TOGGLE_ADV_BUTTON,
											false,
											BUTTON_PULL,
											button_evt_handler
											}
			};

	ret_code_t err_code = app_button_init(buttons, ARRAY_SIZE(buttons), BUTTON_DETECTION_DELAY);
	APP_ERROR_CHECK(err_code);
}

/**@brief Function for enabling button input.
 */
static void buttons_enable(void) {
	ret_code_t err_code = app_button_enable();
	APP_ERROR_CHECK(err_code);
}

/**@brief Function for disabling button input. */
//static void buttons_disable(void)
//{
//    ret_code_t err_code = app_button_disable();
//    APP_ERROR_CHECK(err_code);
//}
/**@brief Function for handling events from the button handler module.
 *
 * @param[in] pin_no        The pin that the event applies to.
 * @param[in] button_action The button action (press/release).
 */
static void button_evt_handler(uint8_t pin_no, uint8_t button_action) {
	if (button_action == 0) {
		return;
	}
	switch (pin_no) {
	case TOGGLE_SCAN_BUTTON: {
		if (m_scan_active) {
			scan_stop();
		} else {
			scan_start();
		}
	}
		break;

	case TOGGLE_ADV_BUTTON: {
		if (m_advertising_active) {
			advertising_stop();
		} else {
			advertising_start();
		}
	}
		break;
#ifdef ENABLE_TOF
		case TOGGLE_TOF_BUTTON:
#warning TODO
		break;
#endif
	default:
		break;
	}
}

static void wait_for_event(void) {
	(void) sd_app_evt_wait();
}

int main(void) {
	initialize_leds();
	timer_init();
	initialize_uart();
	buttons_init();
	ble_stack_init();
	gap_params_init();

	reset_network();
	advertising_data_set();

//#ifdef DNRF52840_XXAA
//    (void) sd_ble_gap_tx_power_set(9);
//#else
//    (void) sd_ble_gap_tx_power_set(4);
//#endif

	preferred_phy_set(&m_test_params.phys);

#ifdef ENABLE_TOF
	tof_init();
	ifs_offset_ticks = calculate_ifs_offset(m_test_params.phys);
#endif
	buttons_enable();

	application_timers_start();

	send_ready();

	while (1) {
		wait_for_event();
	}
}

/**
 * @}
 */
