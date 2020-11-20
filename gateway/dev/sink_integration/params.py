from recordtype import recordtype
from enum import IntEnum, unique
import logging
import os


"""-------------------------------COMMUNICATION PARAMS----------------------------"""
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


"""---------------------------------MQTT---------------------------------------"""
MQTT_BROKER="iot.smartcommunitylab.it"
# MQTT_BROKER="localhost"
MQTT_PORT=1883
#DEVICE_TOKEN="iOn25YbEvRBN9sp6xdCd"     #vela-gateway
# DEVICE_TOKEN="9ovXYK9SOMgcbaIrWvZ2"     #vela-gateway-test
DEVICE_TOKEN="Xk5yuRNrGlcFHoWhC3AW"     #gm-gateway-test

TELEMETRY_TOPIC="v1/gateway/telemetry"
CONNECT_TOPIC="v1/gateway/connect"


""" ----------------------------NETWORK----------------------------------------"""
MAX_TRICKLE_C_VALUE = 256
CONNECT_TOPIC="v1/gateway/connect"

REBOOT_INTERVAL=0.5

CLEAR_CONSOLE = False #True

NETWORK_PERIODIC_CHECK_INTERVAL = 2 # check interval for node disappeared in the network
NODE_TIMEOUT_S = 60*1 # timeout before declaring a node as disappeared

SINGLE_NODE_REPORT_SIZE = 9      #must be the same as in common/inc/contraints.h

MAX_REPORTS_PER_PACKET = 5       # max number of beacons in a message, if bigger it get splitted
MAX_PACKET_PAYLOAD = SINGLE_NODE_REPORT_SIZE*MAX_REPORTS_PER_PACKET

NETWORK_STATUS_POLL_INTERVAL_DEF = 60

"""-------------------------------STRUCTURES----------------------------- """
ContactData = recordtype("ContactData", "nodeid lastRSSI maxRSSI pktCounter")
MessageSequence = recordtype("MessageSequence","timestamp,nodeid,lastPktnum,sequenceSize,datacounter,datalist,latestTime")
NodeDropInfo = recordtype("NodeDropInfo", "nodeid, lastpkt")

"""---------------------------------UART--------------------------------- """
BAUD_RATE = 1000000 #57600 #921600 #1000000
TIMEOUT = 10
SERIAL_PORT = "/dev/ttyACM0" #"/dev/ttyACM0" #"/dev/ttyS0"

START_CHAR = '02'
endchar = 0x0a
SER_END_CHAR = endchar.to_bytes(1, byteorder='big', signed=False)

TANSMIT_ONE_CHAR_AT_THE_TIME=True   #Setting this to true makes the communication from the gateway to the sink more reliable. Tested with an ad-hoc firmware. When this is removed the sink receives corrupted data much more frequently

if TANSMIT_ONE_CHAR_AT_THE_TIME:
    SUBCHUNK_INTERVAL=0.1
    CHUNK_INTERVAL_ADD=0.1
else:
    SUBCHUNK_INTERVAL=0.02 #with 0.1 it is stable, less who knows? 0.03 -> no error in 3 ota downloads. 0.02 no apparten problem...
    CHUNK_INTERVAL_ADD=0.01

TIMEOUT_INTERVAL = 300

"""-----------------------------OTA----------------------------------------"""
REBOOT_DELAY_S=20
MAX_OTA_ATTEMPTS=100

METADATA_LENGTH=14
MAX_OTA_TIMEOUT_COUNT=100
MAX_CHUNK_SIZE=256                  #must be the same as OTA_BUFFER_SIZE in ota-download.h and OTA_CHUNK_SIZE in sink_receiver.c

FIRMWARE_FILENAME = "./../../../network_board/integrate_sender_uart/vela_node_ota"
CHUNK_DOWNLOAD_TIMEOUT=7
MAX_SUBCHUNK_SIZE=128               #MAX_SUBCHUNK_SIZE should be always strictly less than MAX_CHUNK_SIZE (if a chunk doesn't get spitted problems might arise, the case is not managed)

"""--------------------------OCTAVE proximity detection -------------------"""
OCTAVE_LAUNCH_COMMAND="/usr/bin/flatpak run org.octave.Octave -qf" #octave 5 is required. Some distro install octave 4 as default, to overcome this install octave through flatpak
DETECTOR_OCTAVE_SCRIPT = "run_detector.m"
OCTAVE_FILES_FOLDER = "../data_plotting/matlab_version" 
ENABLE_PROCESS_OUTPUT_ON_CONSOLE=True # enable the output of OCTAVE to be displayed in the terminal
PROXIMITY_DETECTOR_POLL_INTERVAL=60 # slot of time [s] between two proximity detection executions. However, user can start the process via the command line
EVENTS_FILE_JSON="json_events.txt"

""" ---------------------------LOG------------------------------------------"""
printVerbosity = 10
LOG_FOLDER_PATH = os.path.dirname(os.path.realpath(__file__))+'/log/'

contact_log_file_folder=LOG_FOLDER_PATH 

LOG_LEVEL = logging.DEBUG
octave_to_log_folder_r_path=os.path.relpath(contact_log_file_folder, OCTAVE_FILES_FOLDER)
