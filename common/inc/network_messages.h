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


#define NET_MESS_MSGTYPE_LEN    2
#define NET_MESS_MSGLEN_LEN     1

#define NET_MESS_BUFF_SIZE      SERIAL_LINE_CONF_BUFSIZE

typedef enum{
    network_new_sequence =          0x0100,
    network_active_sequence =       0x0101,
    network_last_sequence =         0x0102,
    network_bat_data =              0x0200,
    network_set_time_between_sends =0x0601,
    network_request_ping =          0xF000,
    network_respond_ping =          0xF001,
    network_keep_alive =            0xF010,
    nordic_turn_bt_off =            0xF020,
    nordic_turn_bt_on =             0xF021,
    nordic_turn_bt_on_w_params =    0xF022,
    nordic_turn_bt_on_low =         0xF023, //deprecated
    nordic_turn_bt_on_def =         0xF024,
    nordic_turn_bt_on_high =        0xF025, //deprecated
    ti_set_batt_info_int =          0xF026,
    nordic_reset =                  0xF027,
    ti_set_keep_alive =             0xF801,
} pkttype_t;

typedef struct
{
    uint16_t   data_len;    /**< Length of valid data. in this payload*/
    uint8_t   p_data[NET_MESS_BUFF_SIZE];      /**< Pointer to data. */
} payload_data_t;

typedef struct
{
   pkttype_t pkttype;
   uint8_t pktnum;
   payload_data_t payload;
} network_message_t;

#endif
