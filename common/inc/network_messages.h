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


#ifndef NETWORK_MESSAGES_H
#define NETWORK_MESSAGES_H

#define HEADER_SIZE 2

typedef enum{
	network_new_sequence = 0x0100,
	network_active_sequence = 0x0101,
	network_last_sequence = 0x0102,
	network_request_ping = 0xF000,
	network_respond_ping = 0xF001,
	network_bat_data = 0x0200
} pkttype_t;

typedef struct
{
    uint16_t   data_len;    /**< Length of valid data. */
    uint8_t   p_data[49];      /**< Pointer to data. */
} payload_data_t;

struct network_message_t {
   pkttype_t pkttype;
   uint8_t pktnum;
   payload_data_t payload;
};

#endif
