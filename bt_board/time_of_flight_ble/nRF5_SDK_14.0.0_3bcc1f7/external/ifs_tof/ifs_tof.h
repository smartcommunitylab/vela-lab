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
*
*
* author: Davide Giovanelli 2017 - davigiov88@gmail.com
*/


#ifndef IFS_TOF_H__
#define IFS_TOF_H__


#include "ble.h"
#include <stdint.h>
#include <stdbool.h>
//#include "ble_db_discovery.h"
//#include "nrf_ble_gatt.h"
#include "AHRS_lib.h"
#include "app_error.h"
#include "sdk_config.h"

#ifndef SDK_VERSION
#error "SDK_VERSION must be defined to be 13 or 14"
#endif
#if SDK_VERSION != 13 && SDK_VERSION != 14
#error "the ifs_tof module is written to work only with version 13 or 14 of the SDK"
#endif


#if SDK_VERSION == 14
#include "nrf_sdh_ble.h"
#endif

#define SUPERVISION_TIMEOUT_US					700 //ATTENTION: setting this to value less than 700 may create problems when more than one node is connected
#define BLE_IFS_TOF_BLE_OBSERVER_PRIO			3

#ifndef IFS_TOF_EGU_INSTANCE
#define IFS_TOF_EGU_INSTANCE							3
#endif
#ifndef IFS_TOF_EGU_CHANNEL
#define IFS_TOF_EGU_CHANNEL								0
#endif
#ifndef IFS_TOF_TIMER_INSTANCE
#define IFS_TOF_TIMER_INSTANCE							2
#endif
//defines used for calculate the expected offset (composed by IFS and all delays introduced by hardware)
#define T_IFS_us								150
#define ACCESS_ADDRESS_LENGTH_bit 				32
#define PREAMBLE_LEGNTH_us  					8
#define TIMER_START_DELAY_us 					0.25
#define PDU_LENGTH_bit 							16 //only the header for now, the test uses empty packets
#define CRC_LENGTH_bit 							24
#define RX_CHAIN_DELAY_1MPHY_us 				9.4
#define RX_CHAIN_DELAY_2MPHY_us 				5

#define IFS_TIMER_TICK_FREQUENCY_MHZ			16

//values of NRF_RADIO->STATE register (only used values are defined)
#define RADIO_STATE_TX							11
#define RADIO_STATE_RX							3

typedef enum
{
	IFS_TOF_EVT_DISTANCE_READY,
} ifs_tof_evt_type_t;

typedef struct
{
	uint16_t ifs_duration_ticks;
	int8_t rssi;
	uint32_t frequency_reg;
	uint32_t access_address;
	uint32_t timestamp;
}
ifs_tof_sample_t;

typedef struct ifs_tof ifs_tof_t;

typedef struct
{
	ifs_tof_evt_type_t evt_type;       //!<  Type of the event. */
	ifs_tof_t *p_ctx;
} ifs_tof_evt_t;

typedef void (*ifs_tof_evt_handler_t) (	ifs_tof_evt_t    * p_evt);

typedef struct ifs_tof
{
	uint16_t conn_handle;
	uint32_t ce_counter;
	ifs_tof_sample_t ifs_tof_sample;
	ifs_tof_evt_handler_t evt_handler;
	KF_t kf;
	Distance_Data distance_buff;
	float last_d_estimation;
	float last_v_estimation;
} ifs_tof_t;

uint32_t ifs_tof_init(ifs_tof_t * p_buff, uint8_t max_buff_size);

uint32_t ifs_tof_register_conn_handle(const uint16_t conn_handle, ifs_tof_evt_handler_t evt_handler);
uint32_t ifs_tof_unregister_conn_handle(uint16_t connection_handle);
uint16_t ifs_tof_get_number_of_registered_handles(void);

ret_code_t ifs_tof_get_last_sample(ifs_tof_t * p_ctx, ifs_tof_sample_t* sample);


void ifs_tof_init_struct(ifs_tof_t * p_ctx);
void ifs_tof_init_struct_buff(ifs_tof_t * p_ctx, uint16_t buff_size);
void ifs_tof_proximity_reset(void);

uint32_t ifs_tof_enable_module(void);
uint32_t ifs_tof_disable_module(void);

#if SDK_VERSION == 13
void ifs_tof_on_ble_evt(const ble_evt_t * p_ble_evt);
#endif

#endif // IFS_TOF_H__

