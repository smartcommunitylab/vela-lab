from collections import deque
import params as par

mqtt_client = None
mqtt_connected=False

n_chunks_in_firmware = None

dropCounter = 0

nodeDropInfoList = []
messageSequenceList = []

deliverCounter = 0

ser = None # the serial used for the communication
net  = None # the network class, which the code uses to do all the prints


no_of_sub_chunks_in_this_chunk = 0

headerDropCounter = 0

# queue shared between the launcher of proximity detection, that just append a contact file to the list, and the thread that executes
# proximity detection. If the list is empty, the octave program is not launched.
proximity_detector_in_queue = deque()  

network_status_poll_interval=par.NETWORK_STATUS_POLL_INTERVAL_DEF


"""--------------------------VARIOUS LOGGERS-----------------------------"""
consoleLogger = None
consolelog_handler = None

UARTLogger = None
uartlog_handler = None

contactLogger = None
contactlog_handler = None

errorLogger = None
errorlog_handler = None

filenameContactLog = None

"""-------------------------BLUETOOTH PARAMETERS-------------------------"""
btPreviousTime = 0
btToggleBool = True

