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


#ifndef CONSTRAINTS_H
#define CONSTRAINTS_H
typedef struct
{
    uint8_t  * p_data;      /**< Pointer to data. */
    uint16_t   data_len;    /**< Length of data. */
} data_t;

#define MAX_NUMBER_OF_BT_BEACONS 		100
/* Maximum packetsize before fragmentation occurs is 54 bytes with llsec disabled.
   54 bytes - Header size = 50 bytes. Highest power of 9 under 50 is 45 bytes per packet,
   which is 5 reports per packet */ 
#define SINGLE_NODE_REPORT_SIZE 		9
#define MAX_REPORTS_PER_PACKET 5
#define MAX_PACKET_SIZE MAX_REPORTS_PER_PACKET * SINGLE_NODE_REPORT_SIZE

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

#endif
