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


typedef struct
{
    uint8_t  * p_data;      /**< Pointer to data. */
    uint16_t   data_len;    /**< Length of data. */
} data_t;


#define MAX_NUMBER_OF_BT_BEACONS 		100

/*EDDYSTONE DEFINES*/
#define EDDYSTONE_SERVICE_UUID_16BIT	0xFEAA
#define VALID_EDDYSTONE_NAMESPACES 		{ 0x39, 0x06, 0xbf, 0x23, 0x0e, 0x28, 0x85, 0x33 ,0x8f, 0x44 }
#define CLIMB_BEACON_NAMESPACE			{ 0x39, 0x06, 0xbf, 0x23, 0x0e, 0x28, 0x85, 0x33 ,0x8f, 0x44 }
#define EDDYSTONE_NAMESPACE_ID_LENGTH	10
#define EDDYSTONE_INSTANCE_ID_LENGTH	6
#define EDDYSTONE_UID_FRAME_TYPE		0x00
#define EDDYSTONE_URL_FRAME_TYPE		0x10
#define EDDYSTONE_TLM_FRAME_TYPE		0x20
#define EDDYSTONE_EID_FRAME_TYPE		0x30

#define TI_MANUFACTURER_UUID			0x000D
