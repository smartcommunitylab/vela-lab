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
#include "nrf_cli.h"
#include "nrf_cli_rtt.h"
#include "nrf_cli_uart.h"

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

#include "ifs_tof.h"
#include "ble_dists.h"

#include "ble_db_discovery.h"
#include "nrf_ble_gatt.h"
//with conn interval = 25ms we can get 40 ToF samples per sec with three nodes. With more nodes only 20 ToF samples per sec are generated (connection events are skipped by the softdevice!)

#define CONN_INTERVAL_DEFAULT           (uint16_t)(MSEC_TO_UNITS(20, UNIT_1_25_MS))    /**< Default connection interval used at connection establishment by central side. */

#define CONN_INTERVAL_MIN               (uint16_t)(MSEC_TO_UNITS(20, UNIT_1_25_MS))    /**< Minimum acceptable connection interval, in 1.25 ms units. */
#define CONN_INTERVAL_MAX               (uint16_t)(MSEC_TO_UNITS(60, UNIT_1_25_MS))    /**< Maximum acceptable connection interval, in 1.25 ms units. */
#define CONN_SUP_TIMEOUT                (uint16_t)(MSEC_TO_UNITS(700,  UNIT_10_MS))    /**< Connection supervisory timeout (4 seconds). */
#define SLAVE_LATENCY                   0                                               /**< Slave latency. */

#define SCAN_ADV_LED                    BSP_BOARD_LED_0
#define READY_LED                       BSP_BOARD_LED_1
#define PROGRESS_LED                    BSP_BOARD_LED_2
#define DONE_LED                        BSP_BOARD_LED_3

#define BOARD_TESTER_BUTTON             BSP_BUTTON_2                                    /**< Button to press at beginning of the test to indicate that this board is connected to the PC and takes input from it via the UART. */
#define BOARD_DUMMY_BUTTON              BSP_BUTTON_3                                    /**< Button to press at beginning of the test to indicate that this board is standalone (automatic behavior). */
#define BUTTON_DETECTION_DELAY          APP_TIMER_TICKS(50)                             /**< Delay from a GPIOTE event until a button is reported as pushed (in number of timer ticks). */

#define APP_BLE_CONN_CFG_TAG            1                                               /**< A tag that refers to the BLE stack configuration. */
#define APP_BLE_OBSERVER_PRIO           1                                               /**< Application's BLE observer priority. You shouldn't need to modify this value. */
#define L2CAP_HDR_LEN                   4                                               /**< L2CAP header length. */



#define MAXIMUM_NETWORK_SIZE			NRF_SDH_BLE_TOTAL_LINK_COUNT
#define MAXIMUM_NUMBER_OF_PERIPH_CONN	NRF_SDH_BLE_PERIPHERAL_LINK_COUNT
#define MAXIMUM_NUMBER_OF_CENTRAL_CONN	6//IFT_TOF_MAX_NUM_OF_CENTRAL_LINKS

//#define DISABLE_ADV
//#define ENABLE_PHY_UPDATE
#define ENABLE_GATT
#define USE_NANOSECOND

#define TCS_UUID_LIT_END {0x42, 0x00, 0x74, 0xa9, 0xff, 0x52, 0x10, 0x9b, 0x33, 0x49, 0x35, 0x9b, 0x00, 0x01, 0x68, 0xef}; //little endian!!

#define NRF_LOG_FLOAT_MARKER_HA "%s%d.%04d" //high accuracy version of NRF_LOG_FLOAT_MARKER

#ifdef ENABLE_PHY_UPDATE
#if defined(S132)
#undef ENABLE_PHY_UPDATE
#warning "Phy update is not available in sofdevice S132, then phy and won't be updated"
#endif
#endif

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
    bool 			continuous_test;			/**< if set to true the test keeps going on (works without data stream) */
} test_params_t;

/**@brief Variable length data encapsulation in terms of length and pointer to data. */
typedef struct
{
    uint8_t  * p_data;      /**< Pointer to data. */
    uint16_t   data_len;    /**< Length of data. */
} data_t;

typedef struct
{
    uint16_t  			conn_handle;
    uint8_t             local_role;
	ble_gap_addr_t 		bd_address;
} node_t;

#ifdef ENABLE_GATT
NRF_BLE_GATT_DEF(m_gatt);                   /**< GATT module instance. */
BLE_DB_DISCOVERY_DEF(m_ble_db_discovery);   /**< DB discovery module instance. */
BLE_DISTS_DEF(m_dists);                                                 /**< Heart rate service instance. */
#endif

//static nrf_ble_amtc_t     m_amtc;
//static nrf_ble_amts_t     m_amts;
//NRF_SDH_BLE_OBSERVER(m_amtc_ble_obs, BLE_AMTC_BLE_OBSERVER_PRIO, nrf_ble_amtc_on_ble_evt, &m_amtc);
//NRF_SDH_BLE_OBSERVER(m_amts_ble_obs, BLE_AMTS_BLE_OBSERVER_PRIO, nrf_ble_amts_on_ble_evt, &m_amts);
//#endif


NRF_CLI_UART_DEF(cli_uart, 0, 64, 16);
NRF_CLI_RTT_DEF(cli_rtt);
NRF_CLI_DEF(m_cli_uart, "throughput example:~$ ", &cli_uart.transport, '\r', 4);
NRF_CLI_DEF(m_cli_rtt,  "throughput example:~$ ", &cli_rtt.transport,  '\n', 4);

static board_role_t volatile m_board_role  = NOT_SELECTED;

static bool volatile m_run_test = false;

#ifdef ENABLE_GATT
static bool volatile m_mtu_exchanged;
static bool volatile m_data_length_updated;
#endif

#ifdef ENABLE_PHY_UPDATE
static bool volatile m_phy_updated;
#endif
static bool volatile m_conn_interval_configured;
//static uint8_t m_gap_role = BLE_GAP_ROLE_INVALID;


// Name to use for advertising and connection.
static char const m_target_periph_name[] = DEVICE_NAME;
static char const m_target_periph_name_alt[] = {'C',
												'L',
												'I',
												'M',
												'B',
												'L'};

static const uint8_t			  m_target_uuid_lit_end[] = TCS_UUID_LIT_END;

static uint8_t m_no_of_central_links = 0;
static uint8_t m_no_of_peripheral_links = 0;
static ifs_tof_t m_ifs_tof[MAXIMUM_NUMBER_OF_CENTRAL_CONN];
static uint32_t ifs_offset_ticks;
static bool allow_new_connections = false;
static bool pending_connection_req = false;

uint16_t total_number_of_connection_events;
bool m_scan_active = false, m_advertising_active = false;
static node_t m_network[MAXIMUM_NETWORK_SIZE];

// Test parameters.
// Settings like ATT MTU size are set only once on the dummy board.
// Make sure that defaults are sensible.
static test_params_t m_test_params =
{
    .att_mtu                  = BLE_GATT_ATT_MTU_DEFAULT, //NRF_SDH_BLE_GATT_MAX_MTU_SIZE,
    .conn_interval            = CONN_INTERVAL_DEFAULT,
    .data_len_ext_enabled     = false,
    .conn_evt_len_ext_enabled = false,
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
static ble_gap_scan_params_t const m_scan_param =
{
    .active         = 0x00,
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


void test_terminate(void);
static void scan_start(void);
static void advertising_start(void);
static void advertising_stop(void);
void ifs_tof_evt_handler(ifs_tof_evt_t * p_evt);
static void scan_stop(void);
static bool is_link_present(ble_gap_evt_adv_report_t const * p_adv_report);
static node_t* add_node_to_network(const ble_gap_evt_t *p_evt);
node_t* get_first_free_network_pos(void);
node_t* get_network_pos_by_conn_handle(uint16_t target_conn_h);
void test_begin(void);
static void test_run(void);
static bool is_test_ready();

static void reset_network(void){
	memset(m_network, 0x00, MAXIMUM_NETWORK_SIZE*sizeof(node_t));
	for(uint8_t n = 0; n < MAXIMUM_NETWORK_SIZE; n++){
		m_network[n].conn_handle = BLE_CONN_HANDLE_INVALID;
	}
}

static node_t* add_node_to_network(const ble_gap_evt_t *p_evt){
	node_t* p_node = get_first_free_network_pos();
	if(p_node == NULL){
		return NULL;
	}

	p_node->conn_handle = p_evt->conn_handle;
	memcpy(&p_node->bd_address, &p_evt->params.connected.peer_addr, sizeof(ble_gap_addr_t));
	p_node->local_role = p_evt->params.connected.role;
	return p_node;
}

static void remove_node_from_network(const ble_gap_evt_t *p_evt){
	node_t* p_node = get_network_pos_by_conn_handle(p_evt->conn_handle);
	if(p_node == NULL){
		return;
	}

	p_node->conn_handle = BLE_CONN_HANDLE_INVALID;
	memset(&p_node->bd_address, 0x00, sizeof(ble_gap_addr_t));
	p_node->local_role = BLE_GAP_ROLE_INVALID;
	return;
}

node_t* get_first_free_network_pos(void){
	return get_network_pos_by_conn_handle(BLE_CONN_HANDLE_INVALID); //a free position is marked with conn_handle == BLE_CONN_HANDLE_INVALID
}

node_t* get_network_pos_by_conn_handle(uint16_t target_conn_h){
	for(uint8_t n = 0; n < MAXIMUM_NETWORK_SIZE; n++){
		if(m_network[n].conn_handle == target_conn_h){
			return &m_network[n];
		}
	}
	return NULL;
}

static uint32_t calculate_ifs_offset(ble_gap_phys_t actual_phy) {
//#ifdef S140
	uint8_t data_rate_mbps;
	switch (actual_phy.rx_phys) {
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


char const * phy_str(ble_gap_phys_t phys)
{
    static char const * str[] =
    {
        "1 Mbps",
        "2 Mbps",
        "Coded",
        "Unknown"
    };

    switch (phys.tx_phys)
    {
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

uint32_t bd_addr_to_string(const ble_gap_addr_t *p_addr, char* p_str) {
	return sprintf(p_str, "%02X:%02X:%02X:%02X:%02X:%02X", p_addr->addr[5], p_addr->addr[4], p_addr->addr[3], p_addr->addr[2], p_addr->addr[1],
			p_addr->addr[0]);
}

void tof_results_print(ifs_tof_t * p_ctx) {
//	if (m_board_role == BOARD_TESTER)
	{
		char c;
		if (m_test_params.continuous_test) {
			c = 's'; //when the test is continuous just print the summary
		}else{
			c = 'p';
		}

		if (c == 'p' || c == 's') {
			int16_t d = (p_ctx->last_d_estimation)*10;
			ble_dists_estimated_distance_send(&m_dists, p_ctx->conn_handle, d);


			static char addr_str[30]; //18 should be enough, but it mess up

			node_t* m_node = get_network_pos_by_conn_handle(p_ctx->conn_handle);
			bd_addr_to_string(&m_node->bd_address, addr_str);

			char line[300];
			sprintf(line, "bd_addr = %s distance = "NRF_LOG_FLOAT_MARKER" m, velocity = "NRF_LOG_FLOAT_MARKER" m/s\n\r",addr_str,NRF_LOG_FLOAT(p_ctx->last_d_estimation),NRF_LOG_FLOAT(p_ctx->last_v_estimation));
			NRF_LOG_INFO("%s",nrf_log_push(line));
		}
	}
}

static void instructions_print(void)
{
    NRF_LOG_INFO("Type 'config' to change the configuration parameters.\n\r");
    NRF_LOG_FLUSH();
    NRF_LOG_INFO("You can use the TAB key to autocomplete your input.\n\r");
    NRF_LOG_FLUSH();
    NRF_LOG_INFO("Type 'r' when you are ready to run the test.\n\r");
    NRF_LOG_FLUSH();
}


/**@brief Parses advertisement data, providing length and location of the field in case
 *        matching data is found.
 *
 * @param[in]  Type of data to be looked for in advertisement data.
 * @param[in]  Advertisement report length and pointer to report.
 * @param[out] If data type requested is found in the data report, type data length and
 *             pointer to data will be populated here.
 *
 * @retval NRF_SUCCESS if the data type is found in the report.
 * @retval NRF_ERROR_NOT_FOUND if the data type could not be found.
 */
static uint32_t adv_report_parse(uint8_t type, data_t * p_advdata, data_t * p_typedata)
{
    uint32_t  index = 0;
    uint8_t * p_data;

    p_data = p_advdata->p_data;

    while (index < p_advdata->data_len)
    {
        uint8_t field_length = p_data[index];
        uint8_t field_type   = p_data[index + 1];

        if (field_type == type)
        {
            p_typedata->p_data   = &p_data[index + 2];
            p_typedata->data_len = field_length - 1;
            return NRF_SUCCESS;
        }
        index += field_length + 1;
    }
    return NRF_ERROR_NOT_FOUND;
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
static bool find_adv_name(ble_gap_evt_adv_report_t const * p_adv_report, char const * name_to_find)
{
    ret_code_t err_code;
    data_t     adv_data;
    data_t     dev_name;
    bool       found = false;

    // Initialize advertisement report for parsing.
    adv_data.p_data   = (uint8_t *)p_adv_report->data;
    adv_data.data_len = p_adv_report->dlen;

    // Search for matching advertising names.
    err_code = adv_report_parse(BLE_GAP_AD_TYPE_COMPLETE_LOCAL_NAME, &adv_data, &dev_name);

    if (   (err_code == NRF_SUCCESS)
        && (strlen(name_to_find) == dev_name.data_len)
        && (memcmp(name_to_find, dev_name.p_data, dev_name.data_len) == 0))
    {
        found = true;
    }
    else
    {
        // Look for the short local name if the complete name was not found.
        err_code = adv_report_parse(BLE_GAP_AD_TYPE_SHORT_LOCAL_NAME, &adv_data, &dev_name);

        if (   (err_code == NRF_SUCCESS)
            && (strlen(name_to_find) == dev_name.data_len)
            && (memcmp(m_target_periph_name, dev_name.p_data, dev_name.data_len) == 0))
        {
            found = true;
        }
    }

    return found;
}

//finds the first 16bit uuid included in the adv data
static uint32_t get_adv_128bit_uuid(ble_gap_evt_adv_report_t const * p_adv_report, data_t * uuid_data) {

	ret_code_t err_code;
	data_t adv_data;

	// Initialize advertisement report for parsing.
	adv_data.p_data = (uint8_t *) p_adv_report->data;
	adv_data.data_len = p_adv_report->dlen;

	err_code = adv_report_parse(BLE_GAP_AD_TYPE_128BIT_SERVICE_UUID_MORE_AVAILABLE, &adv_data, uuid_data);

	return err_code;
}

bool is_128bit_uuid_included(ble_gap_evt_adv_report_t const * p_adv_report, uint8_t const *target_uuid){
	ret_code_t err_code;
	bool found = false;
	data_t uuid_data;

	err_code = get_adv_128bit_uuid(p_adv_report, &uuid_data);
	if (err_code == NRF_SUCCESS) {
		if (uuid_data.data_len == 16) {
			if( memcmp(target_uuid, uuid_data.p_data, 16) == 0 ){
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

static bool is_adv_connectable(ble_gap_evt_adv_report_t const * p_adv_report){

//    ret_code_t err_code;
//	data_t flags;
//    data_t     adv_data;
//    adv_data.p_data   = (uint8_t *)p_adv_report->data;
//    adv_data.data_len = p_adv_report->dlen;

    if(p_adv_report->type == BLE_GAP_ADV_TYPE_ADV_IND ){
    	return true;
    }else{
    	return false;
    }

//	err_code = adv_report_parse(BLE_GAP_AD_TYPE_FLAGS, &adv_data, &flags);
//
//	if (err_code != NRF_SUCCESS){
//		return false;
//	}else{
//		return flags.p_data[0] && (BLE_GAP_ADV_FLAG_LE_GENERAL_DISC_MODE | BLE_GAP_ADV_FLAG_BR_EDR_NOT_SUPPORTED);
//	}
}


static bool is_link_present(ble_gap_evt_adv_report_t const * p_adv_report){

	for(uint8_t n = 0; n < MAXIMUM_NETWORK_SIZE; n++){
		if(m_network[n].conn_handle != BLE_CONN_HANDLE_INVALID){
			if(memcmp(&p_adv_report->peer_addr, &m_network[n].bd_address, sizeof(ble_gap_addr_t) )== 0){
				return true;
			}
		}
	}
	return false;
}
/**@brief Function for handling BLE_GAP_ADV_REPORT events.
 * Search for a peer with matching device name.
 * If found, stop advertising and send a connection request to the peer.
 */
static void on_ble_gap_evt_adv_report(ble_gap_evt_t const * p_gap_evt) {

	if (!is_adv_connectable(&p_gap_evt->params.adv_report)) {
		return;
	}

	if (//!find_adv_name(&p_gap_evt->params.adv_report, m_target_periph_name) &&
			!find_adv_name(&p_gap_evt->params.adv_report, m_target_periph_name_alt)
			//&& !is_128bit_uuid_included(&p_gap_evt->params.adv_report,  m_target_uuid_lit_end)
			)
	{
		return;
	}

	if(is_link_present(&p_gap_evt->params.adv_report)){
		return;
	}

    bsp_board_led_invert(PROGRESS_LED);

//	if(m_board_role == BOARD_DUMMY){
//		return;
//	}

	if (allow_new_connections && m_no_of_central_links < MAXIMUM_NUMBER_OF_CENTRAL_CONN ) {

		static char addr_str[30]; //18 should be enough, but it mess up
		bd_addr_to_string(& p_gap_evt->params.adv_report.peer_addr, addr_str);

		NRF_LOG_INFO("Device \"%s\", bd_addr \"%s\" found, sending a connection request.\n\r", (uint32_t ) m_target_periph_name,addr_str);

		scan_stop();
		advertising_stop();


		// Initiate connection.
		m_conn_param.min_conn_interval = CONN_INTERVAL_DEFAULT;
		m_conn_param.max_conn_interval = CONN_INTERVAL_DEFAULT;

		ble_gap_scan_params_t const m_init_param =
		{
		    .active         = 0x00,
		    .interval       = SCAN_WINDOW,
		    .window         = SCAN_WINDOW,
		    .use_whitelist  = 0x00,
		    .adv_dir_report = 0x00,
		    .timeout        = 0x0000, // No timeout.
		};

		ret_code_t err_code;
		err_code = sd_ble_gap_connect(&p_gap_evt->params.adv_report.peer_addr, &m_init_param, &m_conn_param,APP_BLE_CONN_CFG_TAG);

		if (err_code != NRF_SUCCESS) {
			NRF_LOG_ERROR("sd_ble_gap_connect() failed: 0x%x.\n\r", err_code);
		}else{
			pending_connection_req = true;
		}
	}
}

/**@brief Function for handling BLE_GAP_EVT_CONNECTED events.
 * Save the connection handle and GAP role, then discover the peer DB.
 */
static void on_ble_gap_evt_connected(ble_gap_evt_t const * p_gap_evt)
{
    ret_code_t err_code;
    //m_conn_handle = p_gap_evt->conn_handle;
    //m_gap_role    = p_gap_evt->params.connected.role;
    node_t* p_node = add_node_to_network(p_gap_evt);
    if(p_node == NULL){
    	return;
    }
    bsp_board_leds_off();

    if (p_node->local_role == BLE_GAP_ROLE_PERIPH)
    {
		NRF_LOG_INFO("Connected as a peripheral.\n\r");
		m_no_of_peripheral_links++;
		if (m_no_of_peripheral_links < MAXIMUM_NUMBER_OF_PERIPH_CONN) {
			m_advertising_active = false; //workaround needed
			advertising_start();
		} else {
        	advertising_stop();
        }
    }
    else if (p_node->local_role == BLE_GAP_ROLE_CENTRAL)
    {
        NRF_LOG_INFO("Connected as a central.\n\r");

        pending_connection_req = false;
		m_no_of_central_links++;
        NRF_LOG_INFO("Registering Connection handle %d with ifs_tof module.\n\r", p_gap_evt->conn_handle);
		ifs_tof_register_conn_handle(p_gap_evt->conn_handle, ifs_tof_evt_handler);

		if(m_board_role == BOARD_DUMMY){
			advertising_start();
		}
		if (m_no_of_central_links < MAXIMUM_NUMBER_OF_CENTRAL_CONN) {
			scan_start();
		} else {
			scan_stop();
		}

		if (m_test_params.conn_interval != CONN_INTERVAL_DEFAULT) {
			NRF_LOG_DEBUG("Updating connection parameters..\n\r");
			m_conn_param.min_conn_interval = m_test_params.conn_interval;
			m_conn_param.max_conn_interval = m_test_params.conn_interval;
			err_code = sd_ble_gap_conn_param_update(p_node->local_role, &m_conn_param);

			if (err_code != NRF_SUCCESS) {
				NRF_LOG_ERROR("sd_ble_gap_conn_param_update() failed: 0x%x.\n\r", err_code);
			}
		} else {
			m_conn_interval_configured = true;
		}

	#ifdef ENABLE_PHY_UPDATE
	    if (p_gap_evt->params.connected.role == BLE_GAP_ROLE_PERIPH)
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

	NRF_LOG_INFO("Connection interval: "NRF_LOG_FLOAT_MARKER" ms, Slave latency: %d, Supervision timeout %d ms.\n\r", NRF_LOG_FLOAT(1.25 * p_gap_evt->params.connected.conn_params.max_conn_interval), p_gap_evt->params.connected.conn_params.slave_latency, p_gap_evt->params.connected.conn_params.conn_sup_timeout * 10 );

	//bsp_board_led_on(READY_LED);

    // If some gatt operation are used, next lines must be uncommented to trigger the service discovery.
    // Form the contextual point of view all modules interested to gatt will add the UUID they are interested in using sd_ble_uuid_vs_add(...), while the application (main.c) will start the discovery in the next lines.
    // Discovery events are passed to modules by the application (main.c) inside db_disc_evt_handler(...)
    //memset(&m_ble_db_discovery, 0x00, sizeof(m_ble_db_discovery));
	//NRF_LOG_INFO("Discovering GATT database...\n\r");
    //err_code  = ble_db_discovery_start(&m_ble_db_discovery, p_gap_evt->conn_handle);
	//APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling BLE_GAP_EVT_DISCONNECTED events.
 * Unset the connection handle and terminate the test.
 */
static void on_ble_gap_evt_disconnected(ble_gap_evt_t const * p_gap_evt)
{
	ifs_tof_unregister_conn_handle(p_gap_evt->conn_handle);

    NRF_LOG_DEBUG("Disconnected: reason 0x%x.\n\r", p_gap_evt->params.disconnected.reason);

    node_t * p_node = get_network_pos_by_conn_handle(p_gap_evt->conn_handle);
	if (p_node->local_role == BLE_GAP_ROLE_CENTRAL) {
		m_no_of_central_links--;
	    if (m_run_test)
	    {
	    	static char addr_str[30]; //18 should be enough, but it mess up
	    	bd_addr_to_string(&p_node->bd_address, addr_str);
	        NRF_LOG_INFO("addr_str: \"%s\" GAP disconnection from Central role while test was running.\n\r",addr_str)
	    }
	} else {
		m_no_of_peripheral_links--;
	    if (m_run_test)
	    {
	    	static char addr_str[30]; //18 should be enough, but it mess up
	    	bd_addr_to_string(&p_node->bd_address, addr_str);
	    	NRF_LOG_INFO("addr_str: \"%s\" GAP disconnection from Peripheral role while test was running.\n\r",addr_str)
	    }
	}

	if(!pending_connection_req){
		if(m_board_role == BOARD_DUMMY){
			advertising_start();
		}
		scan_start();
	}
	bsp_board_led_off(READY_LED);

	if(m_no_of_central_links == 0){
		//bsp_board_leds_off();
	//	test_terminate();
	}

	remove_node_from_network(p_gap_evt);
}


/**@brief Function for handling BLE Stack events.
 *
 * @param[in] p_ble_evt  Bluetooth stack event.
 */
static void ble_evt_handler(ble_evt_t const * p_ble_evt, void * p_context)
{
    uint32_t              err_code;
    ble_gap_evt_t const * p_gap_evt = &p_ble_evt->evt.gap_evt;

    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_ADV_REPORT:
            on_ble_gap_evt_adv_report(p_gap_evt);
            break;

        case BLE_GAP_EVT_CONNECTED:
            on_ble_gap_evt_connected(p_gap_evt);
            break;

        case BLE_GAP_EVT_DISCONNECTED:
            on_ble_gap_evt_disconnected(p_gap_evt);
            break;

        case BLE_GAP_EVT_CONN_PARAM_UPDATE:
        {
            m_conn_interval_configured = true;
            NRF_LOG_INFO("Connection interval updated: 0x%x, 0x%x.\n\r",
                p_gap_evt->params.conn_param_update.conn_params.min_conn_interval,
                p_gap_evt->params.conn_param_update.conn_params.max_conn_interval);
        } break;

        case BLE_GAP_EVT_CONN_PARAM_UPDATE_REQUEST:
        {
            // Accept parameters requested by the peer.
            ble_gap_conn_params_t params;
            params = p_gap_evt->params.conn_param_update_request.conn_params;
            err_code = sd_ble_gap_conn_param_update(p_gap_evt->conn_handle, &params);
            APP_ERROR_CHECK(err_code);

            NRF_LOG_INFO("Connection interval updated (upon request): 0x%x, 0x%x.\n\r",
                p_gap_evt->params.conn_param_update_request.conn_params.min_conn_interval,
                p_gap_evt->params.conn_param_update_request.conn_params.max_conn_interval);
        } break;

        case BLE_GATTS_EVT_SYS_ATTR_MISSING:
        {
            err_code = sd_ble_gatts_sys_attr_set(p_gap_evt->conn_handle, NULL, 0, 0);
            APP_ERROR_CHECK(err_code);
        } break;

        case BLE_GATTC_EVT_TIMEOUT: // Fallthrough.
        case BLE_GATTS_EVT_TIMEOUT:
        {
            NRF_LOG_DEBUG("GATT timeout, disconnecting.\n\r");
            err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gap_evt.conn_handle,
                                             BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            APP_ERROR_CHECK(err_code);
        } break;

        case BLE_EVT_USER_MEM_REQUEST:
        {
            err_code = sd_ble_user_mem_reply(p_ble_evt->evt.common_evt.conn_handle, NULL);
            APP_ERROR_CHECK(err_code);
        } break;
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

            ble_gap_phys_t phys = {0};
            phys.tx_phys = p_phy_evt->tx_phy;
            phys.rx_phys = p_phy_evt->rx_phy;

            ifs_offset_ticks = calculate_ifs_offset(phys);

            NRF_LOG_INFO("PHY update %s. PHY set to %s.\n\r",
                         (p_phy_evt->status == BLE_HCI_STATUS_CODE_SUCCESS) ?
                         "accepted" : "rejected",
                         phy_str(phys));


        } break;
        case BLE_GAP_EVT_PHY_UPDATE_REQUEST:
        {
            err_code = sd_ble_gap_phy_update(p_gap_evt->conn_handle, &m_test_params.phys);
            APP_ERROR_CHECK(err_code);
        } break;
#endif

        default:
            // No implementation needed.
            break;
    }
}

void on_ifs_tof_buffer_full(ifs_tof_t * p_ctx) {
	bsp_board_led_invert(DONE_LED);
//	tof_samples = TOF_BUFFER_SIZE;
	if (!m_test_params.continuous_test) {
		test_terminate();
	}
	tof_results_print(p_ctx);
}

/**@brief Evt dispatcher for ifs_tof module*/
void ifs_tof_evt_handler(ifs_tof_evt_t * p_evt) {

		switch (p_evt->evt_type) {
		case IFS_TOF_EVT_DISTANCE_READY: {
			on_ifs_tof_buffer_full(p_evt->p_ctx); //TODO:change function name
		}
			break;
		default: {
	//no implementation needed
		}
			break;
		}
}

/**@brief Function for handling Database Discovery events.
 *
 * @details This function is a callback function to handle events from the database discovery module.
 *          Depending on the UUIDs that are discovered, this function should forward the events
 *          to their respective service instances.
 *
 * @param[in] p_evt  Pointer to the database discovery event.
 */
static void db_disc_evt_handler(ble_db_discovery_evt_t * p_evt)
{
	//no module uses gatt, then there is no need of discovering database
}


/**@brief Function for handling events from the GATT library. */
static void gatt_evt_handler(nrf_ble_gatt_t * p_gatt, nrf_ble_gatt_evt_t const * p_evt)
{
#ifdef ENABLE_GATT
    switch (p_evt->evt_id)
    {
        case NRF_BLE_GATT_EVT_ATT_MTU_UPDATED:
        {
            m_mtu_exchanged = true;
            NRF_LOG_INFO("ATT MTU exchange completed. MTU set to %u bytes.\n\r",
                         p_evt->params.att_mtu_effective);
        } break;

        case NRF_BLE_GATT_EVT_DATA_LENGTH_UPDATED:
        {
            m_data_length_updated = true;
            NRF_LOG_INFO("Data length updated to %u bytes.\n\r", p_evt->params.data_length);
        } break;
    }
#endif
}

/**@brief Function for setting up advertising data. */
static void advertising_data_set(void)
{
#ifndef DISABLE_ADV
    ble_advdata_t const adv_data =
    {
        .name_type          = BLE_ADVDATA_FULL_NAME,
        .flags              = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE,
        .include_appearance = false,
    };

    ret_code_t err_code = ble_advdata_set(&adv_data, NULL);
    APP_ERROR_CHECK(err_code);
#endif
}


/**@brief Function for starting advertising. */
static void advertising_start(void)
 {
#ifndef DISABLE_ADV
	if (!m_advertising_active) {
		ble_gap_adv_params_t const adv_params = { .type = BLE_GAP_ADV_TYPE_ADV_IND, .p_peer_addr = NULL, .fp = BLE_GAP_ADV_FP_ANY, .interval =
		ADV_INTERVAL, .timeout = 0, };

		NRF_LOG_INFO("Starting advertising.\n\r");

		//bsp_board_led_on(SCAN_ADV_LED);
		m_advertising_active = true;
		ret_code_t err_code = sd_ble_gap_adv_start(&adv_params, APP_BLE_CONN_CFG_TAG);
		APP_ERROR_CHECK(err_code);
	}
#endif
}

/**@brief Function for starting advertising. */
static void advertising_stop(void) {
#ifndef DISABLE_ADV
	if (m_advertising_active) {
		NRF_LOG_INFO("Stopping advertising.\n\r");

		//bsp_board_led_off(SCAN_ADV_LED);
		m_advertising_active = false;
		ret_code_t err_code = sd_ble_gap_adv_stop();
		APP_ERROR_CHECK(err_code);
	}
#endif
}

/**@brief Function to start scanning. */
static void scan_start(void)
{
    if(!m_scan_active){
        NRF_LOG_INFO("Starting scan.\n\r");

        //bsp_board_led_on(SCAN_ADV_LED);
		m_scan_active = true;
		ret_code_t err_code = sd_ble_gap_scan_start(&m_scan_param);
		APP_ERROR_CHECK(err_code);
    }
}


static void scan_stop(void){
	if (m_scan_active) {
		NRF_LOG_INFO("Stopping scan.\n\r");

		//bsp_board_led_off(SCAN_ADV_LED);
		m_scan_active = false;
		ret_code_t err_code = sd_ble_gap_scan_stop();
		APP_ERROR_CHECK(err_code);
	}
}

static void log_init(void)
{
    ret_code_t err_code = NRF_LOG_INIT(app_timer_cnt_get);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for the LEDs initialization.
 *
 * @details Initializes all LEDs used by the application.
 */
static void leds_init(void)
{
    bsp_board_leds_init();
}


/**@brief Function for the Timer initialization.
 *
 * @details Initializes the timer module. This creates and starts application timers.
 */
static void timer_init(void)
{
    ret_code_t err_code = app_timer_init();
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for enabling button input.
 */
static void buttons_enable(void)
{
    ret_code_t err_code = app_button_enable();
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for disabling button input. */
static void buttons_disable(void)
{
    ret_code_t err_code = app_button_disable();
    APP_ERROR_CHECK(err_code);
}

/**@brief Function for handling events from the button handler module.
 *
 * @param[in] pin_no        The pin that the event applies to.
 * @param[in] button_action The button action (press/release).
 */
static void button_evt_handler(uint8_t pin_no, uint8_t button_action)
{
	//if (allow_new_connections == true && m_run_test == false) {
		switch (pin_no) {
		case BOARD_TESTER_BUTTON: {
			NRF_LOG_INFO("Single test started.\n\r");
			m_test_params.continuous_test = false;
			m_board_role = BOARD_TESTER;
			instructions_print();
			advertising_start();
			scan_start();
		}
			break;

		case BOARD_DUMMY_BUTTON: {
			NRF_LOG_INFO("Continuous test started.\n\r");
			m_test_params.continuous_test = true;
			m_board_role = BOARD_DUMMY;
			test_begin();
			advertising_start();
			scan_start();
		}
			break;

		default:
			break;
		}
//	} else {
//		switch (pin_no) {
//		case I_M_CLOSE_BUTTON: {
//			ifs_tof_proximity_reset();
//		}
//			break;
//		default:
//			break;
//		}
//	}
	buttons_disable();
}

/**@brief Function for initializing the button library.
 */
static void buttons_init(void)
{
   // The array must be static because a pointer to it will be saved in the button library.
    static app_button_cfg_t buttons[] =
    {
        {BOARD_TESTER_BUTTON, false, BUTTON_PULL, button_evt_handler},
        {BOARD_DUMMY_BUTTON,  false, BUTTON_PULL, button_evt_handler}
    };

    ret_code_t err_code = app_button_init(buttons, ARRAY_SIZE(buttons), BUTTON_DETECTION_DELAY);
    APP_ERROR_CHECK(err_code);
}


static void client_init(void)
{
#ifdef ENABLE_GATT
    ret_code_t err_code = ble_db_discovery_init(db_disc_evt_handler);
    APP_ERROR_CHECK(err_code);
#endif
}

/**@brief Function for initializing the BLE stack.
 *
 * @details Initializes the SoftDevice and the BLE event interrupt.
 */
static void ble_stack_init(void)
{
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
}


/**@brief Function for initializing GAP parameters.
 *
 * @details This function sets up all the necessary GAP (Generic Access Profile) parameters of the
 *          device including the device name and the preferred connection parameters.
 */
static void gap_params_init(void)
{
    ret_code_t              err_code;
    ble_gap_conn_sec_mode_t sec_mode;

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);

    err_code = sd_ble_gap_device_name_set(&sec_mode,
                                          (uint8_t const *)DEVICE_NAME,
                                          strlen(DEVICE_NAME));
    APP_ERROR_CHECK(err_code);

    err_code = sd_ble_gap_ppcp_set(&m_conn_param);
    APP_ERROR_CHECK(err_code);
}

/**@brief Function for initializing the GATT library. */
static void gatt_init(void)
{
#ifdef ENABLE_GATT
    ret_code_t err_code = nrf_ble_gatt_init(&m_gatt, gatt_evt_handler);
    APP_ERROR_CHECK(err_code);
#endif
}

static void wait_for_event(void)
{
    (void) sd_app_evt_wait();
}


void preferred_phy_set(ble_gap_phys_t * p_phy)
{
#if defined(S140)
    ble_opt_t opts;
    memset(&opts, 0x00, sizeof(ble_opt_t));
    memcpy(&opts.gap_opt.preferred_phys, p_phy, sizeof(ble_gap_phys_t));
    ret_code_t err_code = sd_ble_opt_set(BLE_GAP_OPT_PREFERRED_PHYS_SET, &opts);
    APP_ERROR_CHECK(err_code);
#endif
    memcpy(&m_test_params.phys, p_phy, sizeof(ble_gap_phys_t));
}


void gatt_mtu_set(uint16_t att_mtu)
{
#ifdef ENABLE_GATT
    ret_code_t err_code;

    m_test_params.att_mtu = att_mtu;

    err_code = nrf_ble_gatt_att_mtu_periph_set(&m_gatt, att_mtu);
    APP_ERROR_CHECK(err_code);

    err_code = nrf_ble_gatt_att_mtu_central_set(&m_gatt, att_mtu);
    APP_ERROR_CHECK(err_code);
#endif
}

void conn_interval_set(uint16_t value)
{
    m_test_params.conn_interval = value;
}


static void conn_evt_len_ext_set(bool status)
{
    ret_code_t err_code;
    ble_opt_t  opt;

    memset(&opt, 0x00, sizeof(opt));
    opt.common_opt.conn_evt_ext.enable = status ? 1 : 0;

    err_code = sd_ble_opt_set(BLE_COMMON_OPT_CONN_EVT_EXT, &opt);
    APP_ERROR_CHECK(err_code);
}


void data_len_ext_set(bool status)
{
#ifdef ENABLE_GATT
    m_test_params.data_len_ext_enabled = status;

    uint8_t data_length = status ? (247 + L2CAP_HDR_LEN) : (23 + L2CAP_HDR_LEN);
    (void) nrf_ble_gatt_data_length_set(&m_gatt, BLE_CONN_HANDLE_INVALID, data_length);
#endif
}

//void data_ext_set(bool status){
//}

//void continuous_test_ext_set(bool status){
//	m_test_params.continuous_test = status;
//}

bool is_tester_board(void)
{
    return (m_board_role == BOARD_TESTER);
}


void current_config_print(nrf_cli_t const * p_cli)
{
    char const * role = (m_board_role == BOARD_TESTER) ? "tester" :
                        (m_board_role == BOARD_DUMMY)  ? "dummy" : "not selected";

    nrf_cli_fprintf(p_cli, NRF_CLI_NORMAL, "==== Current test configuration ====\r\n");
    nrf_cli_fprintf(p_cli, NRF_CLI_NORMAL,
                    "Board role:\t\t%s\r\n"
                    "ATT MTU size:\t\t%d\r\n"
                    "Connection interval:\t%d units\r\n"
                    "Data length ext:\t%s\r\n"
                    "Connection length ext:\t%s\r\n"
                    "Preferred PHY:\t\t%s\r\n"
    				"Data stream :\t\t%s\r\n"
    				"Continuous test:\t%s\r\n",
                    role,
                    m_test_params.att_mtu,
                    m_test_params.conn_interval,
                    m_test_params.data_len_ext_enabled ? "on" : "off",
                    m_test_params.conn_evt_len_ext_enabled ? "on" : "off",
                    phy_str(m_test_params.phys)
                    /*m_test_params.continuous_test	? "on" : "off"*/);
}


void test_begin(void) //called by cli module! see example_cmds.c
{
    NRF_LOG_INFO("Preparing the test.\n\r");
    NRF_LOG_FLUSH();

#ifdef ENABLE_PHY_UPDATE
     m_phy_updated = true;
#endif

//    switch (m_gap_role)
//    {
//        default:
//            // If no connection was established, the role won't be either.
//            // In this case, start both advertising and scanning.
//           // advertising_start();
//            //scan_start();
//        	allow_new_connections = true;
//            break;
//
//        case BLE_GAP_ROLE_PERIPH:
//            advertising_start();
//            //m_test_params.phys.tx_phys = BLE_GAP_PHY_2MBPS;
//            break;
//
//        case BLE_GAP_ROLE_CENTRAL:
//        	allow_new_connections = true;
//            //scan_start();
//            break;
//    }

//	advertising_start();
	allow_new_connections = true;
}


static void test_run(void)
{
	//memset(m_ifs_tof.buffer, 0x00, TOF_BUFFER_SIZE); //reset the buffer, create a function in ifs_tof module
	ifs_tof_enable_module();
}


static bool is_test_ready()
{
    return (
			m_conn_interval_configured
//#ifdef ENABLE_GATT
//            && m_mtu_exchanged
//            && m_data_length_updated
//#endif
#ifdef ENABLE_PHY_UPDATE
            && m_phy_updated
#endif
            && !m_run_test);
}


void test_terminate(void)
{
    m_run_test                 = false;
#ifdef ENABLE_GATT
    m_mtu_exchanged            = false;
    m_data_length_updated      = false;
#endif
#ifdef ENABLE_PHY_UPDATE
    m_phy_updated              = false;
#endif
    m_conn_interval_configured = false;
    ifs_tof_disable_module();
    ifs_tof_init_struct_buff(m_ifs_tof, MAXIMUM_NUMBER_OF_CENTRAL_CONN);

//    if (m_conn_handle != BLE_CONN_HANDLE_INVALID)
//    {
//        NRF_LOG_INFO("Disconnecting...");
//
//        ret_code_t err_code;
//        err_code = sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
//
//        allow_new_connections = false;
//
//        if (err_code != NRF_SUCCESS)
//        {
//            NRF_LOG_ERROR("sd_ble_gap_disconnect() failed: 0x%0x.", err_code);
//        }
//    }
//    else
//    {
//        if (m_board_role == BOARD_TESTER)
//        {
////            m_print_menu = true;
//
//            scan_start();
//        }
//        if (m_board_role == BOARD_DUMMY)
//        {
//            if (m_gap_role == BLE_GAP_ROLE_PERIPH)
//            {
//                advertising_start();
//            }
//            else
//            {
//                //scan_start();
//            }
//        }
//    }

	scan_stop();
	advertising_stop();
	for (uint8_t n = 0; n < MAXIMUM_NETWORK_SIZE; n++) {
		if (m_network[n].conn_handle != BLE_CONN_HANDLE_INVALID) {
			sd_ble_gap_disconnect(m_network[n].conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
		}
	}
	reset_network();
}


void cli_init(void)
{
    if (CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk)
    {
        ret_code_t err_code = nrf_cli_init(&m_cli_rtt, NULL, true, true, NRF_LOG_SEVERITY_DEBUG);
        APP_ERROR_CHECK(err_code);
    }

    nrf_drv_uart_config_t uart_config = NRF_DRV_UART_DEFAULT_CONFIG;
    uart_config.pseltxd = TX_PIN_NUMBER;
    uart_config.pselrxd = RX_PIN_NUMBER;
    uart_config.hwfc    = NRF_UART_HWFC_ENABLED;
    uart_config.pselcts = CTS_PIN_NUMBER;
    uart_config.pselrts = RTS_PIN_NUMBER;

    ret_code_t err_code = nrf_cli_init(&m_cli_uart, &uart_config, true, true, NRF_LOG_SEVERITY_DEBUG);//NRF_LOG_SEVERITY_INFO);
    APP_ERROR_CHECK(err_code);
}


void cli_start(void)
{
    ret_code_t err_code =  nrf_cli_start(&m_cli_uart);
    APP_ERROR_CHECK(err_code);
}


void cli_process(void)
{
    nrf_cli_process(&m_cli_uart);
}

void tof_init(){
	ifs_tof_init(m_ifs_tof, MAXIMUM_NUMBER_OF_CENTRAL_CONN);
	ifs_tof_init_struct_buff(m_ifs_tof, MAXIMUM_NUMBER_OF_CENTRAL_CONN);
}

/**@brief Function for initializing services that will be used by the application.
 *
 * @details Initialize the Heart Rate, Battery and Device Information services.
 */
static void services_init(void)
{
    ret_code_t     err_code;
    ble_dist_init_t dists_init;

    memset(&dists_init, 0, sizeof(dists_init));

    dists_init.evt_handler                 = NULL;

    // Here the sec level for the Heart Rate Service can be changed/increased.
    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&dists_init.dist_range_attr_md.cccd_write_perm);
    BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&dists_init.dist_range_attr_md.read_perm);
    BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&dists_init.dist_range_attr_md.write_perm);

    err_code = ble_dists_init(&m_dists, &dists_init);
    APP_ERROR_CHECK(err_code);

    // Initialize Battery Service.
//    memset(&bas_init, 0, sizeof(bas_init));
//
//    // Here the sec level for the Battery Service can be changed/increased.
//    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&bas_init.battery_level_char_attr_md.cccd_write_perm);
//    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&bas_init.battery_level_char_attr_md.read_perm);
//    BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&bas_init.battery_level_char_attr_md.write_perm);
//
//    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&bas_init.battery_level_report_read_perm);
//
//    bas_init.evt_handler          = NULL;
//    bas_init.support_notification = true;
//    bas_init.p_report_ref         = NULL;
//    bas_init.initial_batt_level   = 100;
//
//    err_code = ble_bas_init(&m_bas, &bas_init);
//    APP_ERROR_CHECK(err_code);

    // Initialize Device Information Service.
//    memset(&dis_init, 0, sizeof(dis_init));
//
//    ble_srv_ascii_to_utf8(&dis_init.manufact_name_str, (char *)MANUFACTURER_NAME);
//
//    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&dis_init.dis_attr_md.read_perm);
//    BLE_GAP_CONN_SEC_MODE_SET_NO_ACCESS(&dis_init.dis_attr_md.write_perm);
//
//    err_code = ble_dis_init(&dis_init);
//    APP_ERROR_CHECK(err_code);
}

int main(void)
{
    log_init();

    cli_init();
    leds_init();
    timer_init();
    buttons_init();
    ble_stack_init();
    gap_params_init();
    gatt_init();
    services_init();
    advertising_data_set();
//#ifdef DNRF52840_XXAA
//    (void) sd_ble_gap_tx_power_set(9);
//#else
//    (void) sd_ble_gap_tx_power_set(4);
//#endif
    (void) sd_ble_gap_tx_power_set(0);

    reset_network();
    client_init();
    gatt_mtu_set(m_test_params.att_mtu);
    data_len_ext_set(m_test_params.data_len_ext_enabled);
    conn_evt_len_ext_set(m_test_params.conn_evt_len_ext_enabled);
    preferred_phy_set(&m_test_params.phys);

    tof_init();

    cli_start();

    buttons_enable();
    ifs_offset_ticks = calculate_ifs_offset(m_test_params.phys);

    NRF_LOG_INFO("BLE Time of Flight example started.\n\r");
    NRF_LOG_INFO("Press button 3 for a single test, this may have problems.\n\r");
    NRF_LOG_INFO("Press button 4 for a continuous test.\n\r");

//    NRF_LOG_INFO("Remember: every sample plotted in the terminal corresponds to %u samples averaged.\n\r",TOF_BUFFER_SIZE);
//    NRF_LOG_INFO("The average size is set by TOF_BUFFER_SIZE in ifs_tof.h\n\r");

	for (;;) {

		cli_process();

		if (is_test_ready()) {
			NRF_LOG_INFO("Test started\n\r");
			m_run_test = true;
			test_run();
		}

		if (NRF_LOG_PROCESS() == false){
            wait_for_event();
        }
    }
}


/**
 * @}
 */
