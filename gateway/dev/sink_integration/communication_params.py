from enum import IntEnum, unique
from recordtype import recordtype

@unique
class PacketType(IntEnum):
    ota_start_of_firmware =         0x0020  # OTA start of firmware (sink to node)
    ota_more_of_firmware =          0x0021  # OTA more of firmware (sink to node)
    ota_end_of_firmware =           0x0022  # OTA end of firmware (sink to node)
    ota_ack =                       0x0023  # OTA firmware packet ack (node to sink)
    ota_reboot_node =               0x0024  # OTA reboot (node to sink)
    network_new_sequence =          0x0100  # bluetooth beacons first elements
    network_active_sequence =       0x0101  # bluetooth beacons intermediate elements
    network_last_sequence =         0x0102  # bluetooth beacons last elements
    network_bat_data =              0x0200  #deprecated (merged with keepalive)
    network_set_time_between_send = 0x0601
    network_request_ping =          0xF000
    network_respond_ping =          0xF001
    network_keep_alive =            0xF010
    nordic_turn_bt_off =            0xF020
    nordic_turn_bt_on =             0xF021
    nordic_turn_bt_on_w_params =    0xF022
    nordic_turn_bt_on_low =         0xF023  #deprecated
    nordic_turn_bt_on_def =         0xF024
    nordic_turn_bt_on_high =        0xF025  #deprecated
    ti_set_batt_info_int =          0xF026 #deprecated (merged with keepalive)
    nordic_reset =                  0xF027
    nordic_ble_tof_enable =         0xF030
    ti_set_keep_alive =             0xF801
    #debug_1 =                       0xFFF1  #debug!
    #debug_2 =                       0xFFF2  #debug!
    #debug_3 =                       0xFFF3  #debug!
    #debug_4 =                       0xFFF4  #debug!
    #debug_5 =                       0xFFF5  #debug!
