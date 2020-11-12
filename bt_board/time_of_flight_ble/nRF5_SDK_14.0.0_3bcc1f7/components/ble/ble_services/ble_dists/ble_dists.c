/**
 * Copyright (c) 2012 - 2017, Nordic Semiconductor ASA
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
/* Attention!
 * To maintain compliance with Nordic Semiconductor ASA's Bluetooth profile
 * qualification listings, this section of source code must not be modified.
 */
#include "sdk_common.h"
#if NRF_MODULE_ENABLED(BLE_DISTS)
#include "ble_dists.h"
#include <string.h>
#include "ble_l2cap.h"
#include "ble_srv_common.h"
#define  NRF_LOG_MODULE_NAME ble_dists
#if 0
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
NRF_LOG_MODULE_REGISTER();
#define PRINTF(...) PRINTF(__VA_ARGS__); /*\
                    NRF_LOG_PROCESS()*/
#else
#define PRINTF(...)
#endif

#define OPCODE_LENGTH 1                                                              /**< Length of opcode inside Heart Rate Measurement packet. */
#define HANDLE_LENGTH 2                                                              /**< Length of handle inside Heart Rate Measurement packet. */
#define MAX_DIST_LEN      (NRF_SDH_BLE_GATT_MAX_MTU_SIZE - OPCODE_LENGTH - HANDLE_LENGTH) /**< Maximum size of a transmitted Heart Rate Measurement. */

#define INITIAL_VALUE_DISTANCE                      123                                    /**< Initial Heart Rate Measurement value. */

// EF68xxxx-9B35-4933-9B10-52FFA9740042
#define DISTS_BASE_UUID                  {{0x42, 0x00, 0x74, 0xA9, 0xFF, 0x52, 0x10, 0x9B, 0x33, 0x49, 0x35, 0x9B, 0x00, 0x00, 0x68, 0xEF}} /**< Used vendor specific UUID. */


/**@brief Function for handling the Connect event.
 *
 * @param[in]   p_dists       Heart Rate Service structure.
 * @param[in]   p_ble_evt   Event received from the BLE stack.
 */
static void on_connect(ble_dists_t * p_dists, ble_evt_t const * p_ble_evt)
{
    //p_dists->conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
}


/**@brief Function for handling the Disconnect event.
 *
 * @param[in]   p_dists       Heart Rate Service structure.
 * @param[in]   p_ble_evt   Event received from the BLE stack.
 */
static void on_disconnect(ble_dists_t * p_dists, ble_evt_t const * p_ble_evt)
{
    UNUSED_PARAMETER(p_ble_evt);
    //p_dists->conn_handle = BLE_CONN_HANDLE_INVALID;
}


/**@brief Function for handling write events to the Heart Rate Measurement characteristic.
 *
 * @param[in]   p_dists         Heart Rate Service structure.
 * @param[in]   p_evt_write   Write event received from the BLE stack.
 */
static void on_range_cccd_write(ble_dists_t * p_dists, ble_gatts_evt_write_t const * p_evt_write, uint16_t conn_handle)
{
    if (p_evt_write->len == 2)
    {
        PRINTF("CCCD write to 0x%x%x\r\n",p_evt_write->data[1],p_evt_write->data[0]);
        // CCCD written, update notification state
        if (p_dists->evt_handler != NULL)
        {
            ble_dists_evt_t evt;
            evt.conn_handle = conn_handle;
            if (ble_srv_is_notification_enabled(p_evt_write->data))
            {
                evt.evt_type = BLE_DISTS_EVT_NOTIFICATION_ENABLED;
            }
            else
            {
                evt.evt_type = BLE_DISTS_EVT_NOTIFICATION_DISABLED;
            }

            p_dists->evt_handler(p_dists, &evt);
        }
    }
}


/**@brief Function for handling the Write event.
 *
 * @param[in]   p_dists       Heart Rate Service structure.
 * @param[in]   p_ble_evt   Event received from the BLE stack.
 */
static void on_write(ble_dists_t * p_dists, ble_evt_t const * p_ble_evt)
{
    ble_gatts_evt_write_t const * p_evt_write = &p_ble_evt->evt.gatts_evt.params.write;

    if (p_evt_write->handle == p_dists->dist_handles.cccd_handle)
    {
        on_range_cccd_write(p_dists, p_evt_write, p_ble_evt->evt.common_evt.conn_handle);
    }
}

#if SDK_VERSION == 14
void ble_dists_on_ble_evt(ble_evt_t const * p_ble_evt, void * p_context)
{
    ble_dists_t * p_dists = (ble_dists_t *) p_context;
#endif
#if SDK_VERSION == 13
void ble_dists_on_ble_evt(ble_dists_t * p_dists, ble_evt_t const * p_ble_evt) {
#endif

	switch (p_ble_evt->header.evt_id) {
	case BLE_GAP_EVT_CONNECTED:
		on_connect(p_dists, p_ble_evt);
		break;

	case BLE_GAP_EVT_DISCONNECTED:
		on_disconnect(p_dists, p_ble_evt);
		break;

	case BLE_GATTS_EVT_WRITE:
		on_write(p_dists, p_ble_evt);
		break;

	case BLE_GATTS_EVT_HVN_TX_COMPLETE:
		PRINTF("BLE_GATTS_EVT_HVN_TX_COMPLETE with count = %d\r\n",p_ble_evt->evt.gatts_evt.params.hvn_tx_complete.count);
		break;

	default:
		// No implementation needed.
		break;
	}
}

//static uint32_t model_conf_char_add(ble_dists_t * p_dists, const ble_dist_init_t * p_dists_init) {
//	ble_gatts_char_md_t char_md;
//	ble_gatts_attr_md_t cccd_md;
//	ble_gatts_attr_t attr_char_value;
//	ble_uuid_t ble_uuid;
//	ble_gatts_attr_md_t attr_md;
//	uint8_t initial_distance[2] = { INITIAL_VALUE_DISTANCE, INITIAL_VALUE_DISTANCE };
//
//	memset(&cccd_md, 0, sizeof(cccd_md));
//
//	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.read_perm);
//	cccd_md.write_perm = p_dists_init->dist_model_cfg_attr_md.cccd_write_perm;
//	cccd_md.vloc = BLE_GATTS_VLOC_STACK;
//
//	memset(&char_md, 0, sizeof(char_md));
//
//	char_md.char_props.notify = 1;
//	char_md.char_props.read = 1;
//	char_md.p_char_user_desc = NULL;
//	char_md.p_char_pf = NULL;
//	char_md.p_user_desc_md = NULL;
//	char_md.p_cccd_md = &cccd_md;
//	char_md.p_sccd_md = NULL;
//
//	ble_uuid.type = p_dists->uuid_type;
//	ble_uuid.uuid = BLE_UUID_DISTANCE_ESTIMATION_CHAR;
//
//	//BLE_UUID_BLE_ASSIGN(ble_uuid, BLE_UUID_DISTANCE_ESTIMATION_CHAR);
//
//	memset(&attr_md, 0, sizeof(attr_md));
//
//	attr_md.read_perm = p_dists_init->dist_model_cfg_attr_md.read_perm;
//	attr_md.write_perm = p_dists_init->dist_model_cfg_attr_md.write_perm;
//	attr_md.vloc = BLE_GATTS_VLOC_STACK;
//	attr_md.rd_auth = 0;
//	attr_md.wr_auth = 0;
//	attr_md.vlen = 1;
//
//	memset(&attr_char_value, 0, sizeof(attr_char_value));
//
//	attr_char_value.p_uuid = &ble_uuid;
//	attr_char_value.p_attr_md = &attr_md;
//	attr_char_value.init_len = 2;
//	attr_char_value.init_offs = 0;
//	attr_char_value.max_len = MAX_DIST_LEN;
//	attr_char_value.p_value = initial_distance;
//
//	return sd_ble_gatts_characteristic_add(p_dists->service_handle, &char_md, &attr_char_value, &p_dists->model_cfg_handles);
//}

/**@brief Function for adding the Heart Rate Measurement characteristic.
 *
 * @param[in]   p_dists        Heart Rate Service structure.
 * @param[in]   p_dists_init   Information needed to initialize the service.
 *
 * @return      NRF_SUCCESS on success, otherwise an error code.
 */
static uint32_t estimated_distance_char_add(ble_dists_t * p_dists, const ble_dist_init_t * p_dists_init) {
	ble_gatts_char_md_t char_md;
	ble_gatts_attr_md_t cccd_md;
	ble_gatts_attr_t attr_char_value;
	ble_uuid_t ble_uuid;
	ble_gatts_attr_md_t attr_md;
	uint8_t initial_distance[2] = { INITIAL_VALUE_DISTANCE, INITIAL_VALUE_DISTANCE };

	memset(&cccd_md, 0, sizeof(cccd_md));

	BLE_GAP_CONN_SEC_MODE_SET_OPEN(&cccd_md.read_perm);
	cccd_md.write_perm = p_dists_init->dist_range_attr_md.cccd_write_perm;
	cccd_md.vloc = BLE_GATTS_VLOC_STACK;

	memset(&char_md, 0, sizeof(char_md));

	char_md.char_props.notify = 1;
	char_md.char_props.read = 1;
	char_md.p_char_user_desc = NULL;
	char_md.p_char_pf = NULL;
	char_md.p_user_desc_md = NULL;
	char_md.p_cccd_md = &cccd_md;
	char_md.p_sccd_md = NULL;

	ble_uuid.type = p_dists->uuid_type;
	ble_uuid.uuid = BLE_UUID_DISTANCE_ESTIMATION_CHAR;

//BLE_UUID_BLE_ASSIGN(ble_uuid, BLE_UUID_DISTANCE_ESTIMATION_CHAR);

	memset(&attr_md, 0, sizeof(attr_md));

	attr_md.read_perm = p_dists_init->dist_range_attr_md.read_perm;
	attr_md.write_perm = p_dists_init->dist_range_attr_md.write_perm;
	attr_md.vloc = BLE_GATTS_VLOC_STACK;
	attr_md.rd_auth = 0;
	attr_md.wr_auth = 0;
	attr_md.vlen = 1;

	memset(&attr_char_value, 0, sizeof(attr_char_value));

	attr_char_value.p_uuid = &ble_uuid;
	attr_char_value.p_attr_md = &attr_md;
	attr_char_value.init_len = 2; //hrm_encode(p_dists, INITIAL_VALUE_HRM, encoded_initial_hrm);
	attr_char_value.init_offs = 0;
	attr_char_value.max_len = MAX_DIST_LEN;
	attr_char_value.p_value = initial_distance;

	return sd_ble_gatts_characteristic_add(p_dists->service_handle, &char_md, &attr_char_value, &p_dists->dist_handles);

}


uint32_t ble_dists_init(ble_dists_t * p_dists, const ble_dist_init_t * p_dists_init)
{
    uint32_t   err_code;
    ble_uuid_t ble_uuid;
    ble_uuid128_t tcs_base_uuid = DISTS_BASE_UUID;

    VERIFY_PARAM_NOT_NULL(p_dists);
    VERIFY_PARAM_NOT_NULL(p_dists_init);

    PRINTF("Initializing BLE distance service.\r\n")
    // Initialize service structure
    p_dists->evt_handler                 = p_dists_init->evt_handler;
    //p_dists->conn_handle                 = BLE_CONN_HANDLE_INVALID;
    p_dists->max_hrm_len                 = MAX_DIST_LEN;

    // Add a custom base UUID.
    err_code = sd_ble_uuid_vs_add(&tcs_base_uuid, &p_dists->uuid_type);
    VERIFY_SUCCESS(err_code);

    ble_uuid.type = p_dists->uuid_type;
    ble_uuid.uuid = BLE_UUID_DISTANCE_ESTIMATION_SERVICE;

    // Add the service.
    err_code = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY,
                                        &ble_uuid,
                                        &p_dists->service_handle);

    // Add service
//    BLE_UUID_BLE_ASSIGN(ble_uuid, BLE_UUID_DISTANCE_ESTIMATION_SERVICE);
//
//    err_code = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY,
//                                        &ble_uuid,
//                                        &p_dists->service_handle);

    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    err_code = estimated_distance_char_add(p_dists, p_dists_init);
    if (err_code != NRF_SUCCESS)
    {
        return err_code;
    }

    return NRF_SUCCESS;
}


uint32_t ble_dists_estimated_distance_send(ble_dists_t * p_dists, uint16_t dest_peer_conn_handle ,int16_t distance)
{
    uint32_t err_code;

    // Send value if connected and notifying
    if (dest_peer_conn_handle != BLE_CONN_HANDLE_INVALID)
    {
        uint8_t                encoded_distance[2];
        uint16_t               len;
        uint16_t               hvx_len;
        ble_gatts_hvx_params_t hvx_params;

        len     = 2;//hrm_encode(p_dists, heart_rate, encoded_distance);
        encoded_distance[1] = (uint8_t)(0xFF&(distance>>8));
        encoded_distance[0] = (uint8_t)(0xFF&(distance));
        hvx_len = 2;

        memset(&hvx_params, 0, sizeof(hvx_params));

        hvx_params.handle = p_dists->dist_handles.value_handle;
        hvx_params.type   = BLE_GATT_HVX_NOTIFICATION;
        hvx_params.offset = 0;
        hvx_params.p_len  = &hvx_len;
        hvx_params.p_data = encoded_distance;

        err_code = sd_ble_gatts_hvx(dest_peer_conn_handle, &hvx_params);
        if ((err_code == NRF_SUCCESS) && (hvx_len != len))
        {
        	PRINTF("Wrong data size!\r\n");
        	err_code = NRF_ERROR_DATA_SIZE;
        }

        if (err_code != NRF_SUCCESS){
//            if(err_code == BLE_ERROR_GATTS_SYS_ATTR_MISSING){
//            	PRINTF("err_code = BLE_ERROR_GATTS_SYS_ATTR_MISSING, calling sd_ble_gatts_sys_attr_set\r\n",err_code);
//            	//err_code = sd_ble_gatts_sys_attr_set(p_dists->conn_handle, NULL, 0, 0);
//            }else{
            	PRINTF("err_code = %d\r\n",err_code);
//            }
        }
    }
    else
    {
    	PRINTF("Invalid state!!\r\n");
        err_code = NRF_ERROR_INVALID_STATE;
    }
    return err_code;
}

void ble_hrs_on_gatt_evt(ble_dists_t * p_dists, nrf_ble_gatt_evt_t const * p_gatt_evt)
{

	//TODO: mtu is specific for each connection, then max_hrm_len shall be specific for each connection. Anyway for sending small notifications there is no need of using large MTU
//    if (    (p_dists->conn_handle == p_gatt_evt->conn_handle)
//        &&  (p_gatt_evt->evt_id == NRF_BLE_GATT_EVT_ATT_MTU_UPDATED))
//    {
//        p_dists->max_hrm_len = p_gatt_evt->params.att_mtu_effective - OPCODE_LENGTH - HANDLE_LENGTH;
//    }
}
#endif // NRF_MODULE_ENABLED(BLE_HRS)
