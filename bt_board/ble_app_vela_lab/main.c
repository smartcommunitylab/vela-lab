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
#include "nrf_delay.h"
//#include "nrf_cli.h"
//#include "nrf_cli_rtt.h"
//#include "nrf_cli_uart.h"
#include "spi_util.h"
#include "ble_util.h"
#include "timer_util.h"
#include "nb_report_util.h"

#include "constraints.h"

#include "nrf_log_default_backends.h"

#define  NRF_LOG_MODULE_NAME application
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
NRF_LOG_MODULE_REGISTER();
#define PRINTF(...) NRF_LOG_DEBUG(__VA_ARGS__); /*\
                    NRF_LOG_PROCESS()*/

#define CONN_INTERVAL_DEFAULT 0
#define CONN_INTERVAL_MIN               (uint16_t)(MSEC_TO_UNITS(10, UNIT_1_25_MS))    /**< Minimum acceptable connection interval, in 1.25 ms units. */
#define CONN_INTERVAL_MAX               (uint16_t)(MSEC_TO_UNITS(25, UNIT_1_25_MS))    /**< Maximum acceptable connection interval, in 1.25 ms units. */
#define CONN_SUP_TIMEOUT                (uint16_t)(MSEC_TO_UNITS(100,  UNIT_10_MS))    /**< Connection supervisory timeout (4 seconds). */
#define SLAVE_LATENCY                   0                                               /**< Slave latency. */

#define APP_BLE_OBSERVER_PRIO           1                                               /**< Application's BLE observer priority. You shouldn't need to modify this value. */

static char const m_target_periph_name[] = DEVICE_NAME;
static char const m_target_periph_name_alt1[] = {'C','L','I','M','B','C'};



//BT neighbors list management
void on_scan_response(ble_gap_evt_adv_report_t const * p_adv_report);
void on_adv(ble_gap_evt_adv_report_t const * p_adv_report);


uint32_t get_time_since_last_contact(node_t *p_node);
node_t* get_network_pos_by_bdaddr(const ble_gap_addr_t *target_addr);
node_t* get_network_pos_by_conn_handle(uint16_t target_conn_h);
node_t* get_first_free_network_pos(void);
static void remove_node_from_network(node_t* p_node);
static node_t* add_node_to_network(const ble_gap_evt_t *p_conn_evt, const ble_gap_evt_adv_report_t *p_adv_evt, node_type_t node_type);
static node_t* update_node(const ble_gap_evt_t *p_conn_evt, const ble_gap_evt_adv_report_t *p_adv_evt, node_t * p_node);


//bt callbacks/helpers
static void ble_stack_init(void);
static void ble_evt_handler(ble_evt_t const * p_ble_evt, void * p_context);
static void on_ble_gap_evt_adv_report(ble_gap_evt_t const * p_gap_evt);

// bt functions and stuff
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

//system
static void wait_for_event(void);


void on_scan_response(ble_gap_evt_adv_report_t const * p_adv_report) {
	node_t *node = get_network_pos_by_bdaddr(&p_adv_report->peer_addr);
	if (node != NULL) {
		update_node(NULL, p_adv_report, node);
	} 
}

void on_adv(ble_gap_evt_adv_report_t const * p_adv_report) {
	node_type_t node_type = UNKNOWN_TYPE;
	if (is_known_name(p_adv_report, m_target_periph_name_alt1)) {
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
		node_t *node = get_network_pos_by_bdaddr( &p_adv_report->peer_addr );
		if (node == NULL) {
			add_node_to_network(NULL, p_adv_report, node_type);
		} else {
			update_node(NULL, p_adv_report, node);
		}
	}
}

uint32_t get_time_since_last_contact(node_t *p_node) {
	return app_timer_cnt_diff_compute(app_timer_cnt_get(), p_node->last_contact_ticks);
}

node_t* get_network_pos_by_bdaddr(const ble_gap_addr_t *target_addr) {
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

// uint8_t is_position_free(node_t *p_node) {
// 	return p_node->conn_handle == BLE_CONN_HANDLE_INVALID && p_node->first_contact_ticks == 0;
// }

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
        memcpy(&p_node->bd_address, &p_adv_evt->peer_addr, sizeof(ble_gap_addr_t));
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

	if (p_conn_evt != NULL) {
		memcpy(&p_node->bd_address, &(p_conn_evt->params.connected.peer_addr), sizeof(ble_gap_addr_t));
	}
	p_node->node_type = node_type;
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
		p_node->last_rssi=0;
		p_node->max_rssi=0;
		p_node->beacon_msg_count=0;
	}
	if (p_adv_evt != NULL) {
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

	//set_scan_params(DEFAULT_ACTIVE_SCAN, DEFAULT_SCAN_INTERVAL, DEFAULT_SCAN_WINDOW, DEFAULT_TIMEOUT);
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
	case BLE_GAP_EVT_DISCONNECTED:
	case BLE_GAP_EVT_CONN_PARAM_UPDATE: 
	case BLE_GAP_EVT_CONN_PARAM_UPDATE_REQUEST: 
	case BLE_GATTS_EVT_SYS_ATTR_MISSING: 
	case BLE_GATTC_EVT_TIMEOUT: 
	case BLE_GATTS_EVT_TIMEOUT: 
	case BLE_EVT_USER_MEM_REQUEST: 
	case BLE_GAP_EVT_PHY_UPDATE_REQUEST: 
	
	default:
		// No implementation needed.
		break;
	}
}

/**@brief Function for handling BLE_GAP_ADV_REPORT events.
 * Search for a peer with matching device name.
 * If found, stop advertising and send a connection request to the peer.
 */
static void on_ble_gap_evt_adv_report(ble_gap_evt_t const * p_gap_evt) {

	if(preparing_payload){ //do not update the network during report, just for safety
		return;
	}
	
	if (p_gap_evt->params.adv_report.scan_rsp) {	//if it is a scan response it won't contain eddystone frame. It may contain the name. And it may be valid if the node is already in the list

		return on_scan_response(&p_gap_evt->params.adv_report);
	} else {
		return on_adv(&p_gap_evt->params.adv_report);
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


static void wait_for_event(void) {
	(void) sd_app_evt_wait();
}

/**@brief Function for initializing the nrf log module.
 */
static void log_init(void)
{
    ret_code_t err_code = NRF_LOG_INIT(app_timer_cnt_get);
    APP_ERROR_CHECK(err_code);

    NRF_LOG_DEFAULT_BACKENDS_INIT();

//    if (CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk)
//    {
//        ret_code_t err_code = nrf_cli_init(&m_cli_rtt, NULL, true, true, NRF_LOG_SEVERITY_DEBUG);
//        APP_ERROR_CHECK(err_code);
//    }
}


int main(void) {
    log_init();
	initialize_leds();
	timer_init();
	spi_init();
	ble_stack_init();						
	sd_power_dcdc_mode_set( NRF_POWER_DCDC_ENABLE );
	advertising_init();
	reset_network(); //initialize node structs to correct values
	scan_init();
	
	application_timers_start();

    PRINTF("Running!\n");
	nrf_delay_ms(1000); //delay a bit to allow all the hardware to be ready (not strictly necessary)

	
	while (1) {
        //if (NRF_LOG_PROCESS() == false) // Process logs
        //{
        //    wait_for_event();
        //}
		//wait_for_event();
		nrf_delay_ms(500);
		bsp_board_led_invert(BSP_BOARD_LED_2);
		bsp_board_led_invert(BSP_BOARD_LED_3);
	}
}

/**
 * @}
 */
