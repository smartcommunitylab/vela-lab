MAX_TRICKLE_C_VALUE = 256
CONNECT_TOPIC="v1/gateway/connect"

REBOOT_INTERVAL=0.5

CLEAR_CONSOLE = False #True

NETWORK_PERIODIC_CHECK_INTERVAL = 2 # check interval for node disappeared in the network
NODE_TIMEOUT_S = 60*1 # timeout before declaring a node as disappeared

SINGLE_NODE_REPORT_SIZE = 9      #must be the same as in common/inc/contraints.h

MAX_REPORTS_PER_PACKET = 5       # max number of beacons in a message, if bigger it get splitted
MAX_PACKET_PAYLOAD = SINGLE_NODE_REPORT_SIZE*MAX_REPORTS_PER_PACKET

