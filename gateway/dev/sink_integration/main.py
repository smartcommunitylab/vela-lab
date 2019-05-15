import serial
import time
import datetime
import logging
from recordtype import recordtype
from enum import IntEnum, unique
from datetime import timedelta
import _thread
import readchar
import os
import sys
import threading
from collections import deque
import shutil
import binascii

START_CHAR = '02'
#START_BUF = '2a2a2a'  # this is not really a buffer
BAUD_RATE = 1000000 #921600 #1000000
SERIAL_PORT = "/dev/ttyS0" #"/dev/ttyACM0" #"/dev/ttyS0"

if len(sys.argv)>1:
	SERIAL_PORT=sys.argv[1]


logfolderpath = os.path.dirname(os.path.realpath(__file__))+'/log/'
if not os.path.exists(logfolderpath):
    try:
        os.mkdir(logfolderpath)
        net.addPrint("Log directory not found.")
        net.addPrint("%s Created " % logfolderpath)
    except Exception as e:
        net.addPrint("Can't get the access to the log folder %s." % logfolderpath)
        net.addPrint("Exception: %s" % str(e))


endchar = 0x0a
SER_END_CHAR = endchar.to_bytes(1, byteorder='big', signed=False)

SINGLE_NODE_REPORT_SIZE = 9      #must be the same as in common/inc/contraints.h
MAX_REPORTS_PER_PACKET = 22 #must be the same as in common/inc/contraints.h
MAX_PACKET_PAYLOAD = SINGLE_NODE_REPORT_SIZE*MAX_REPORTS_PER_PACKET

MAX_BEACONS = 100

MAX_ONGOING_SEQUENCES = 10

MAX_TRICKLE_C_VALUE = 256

ser = serial.Serial()
ser.port = SERIAL_PORT
ser.baudrate = BAUD_RATE
ser.timeout = 100

MessageSequence = recordtype("MessageSequence", "timestamp,nodeid,lastPktnum,sequenceSize,datacounter,"
                                                "datalist,latestTime")
messageSequenceList = []
ContactData = recordtype("ContactData", "nodeid lastRSSI maxRSSI pktCounter")

firstMessageInSeq = False

NodeDropInfo = recordtype("NodeDropInfo", "nodeid, lastpkt")
nodeDropInfoList = []

ActiveNodes = []

deliverCounter = 0
dropCounter = 0
headerDropCounter = 0
defragmentationCounter = 0

# Timeout variables
previousTimeTimeout = 0
currentTime = 0
timeoutInterval = 300
timeoutTime = 60

previousTimePing = 0
pingInterval = 115

btPreviousTime = 0
btToggleInterval = 1800
btToggleBool = True

NODE_TIMEOUT_S = 60*10
NETWORK_PERIODIC_CHECK_INTERVAL = 2
CLEAR_CONSOLE = True

# Loggers
printVerbosity = 10

LOG_LEVEL = logging.DEBUG

formatter = logging.Formatter('%(asctime)s %(levelname)s %(message)s', datefmt='%Y/%m/%d %H:%M:%S')
timestr = time.strftime("%Y%m%d-%H%M%S")

nameConsoleLog = "consoleLogger"
filenameConsoleLog = timestr + "-console.log"
fullpathConsoleLog = logfolderpath+filenameConsoleLog
consolloghandler = logging.FileHandler(fullpathConsoleLog)
consolloghandler.setFormatter(formatter)
consoleLogger = logging.getLogger(nameConsoleLog)
consoleLogger.setLevel(LOG_LEVEL)
consoleLogger.addHandler(consolloghandler)

nameUARTLog = "UARTLogger"
filenameUARTLog = timestr + "-UART.log"
fullpathUARTLog = logfolderpath+filenameUARTLog
handler = logging.FileHandler(fullpathUARTLog)
handler.setFormatter(formatter)
UARTLogger = logging.getLogger(nameUARTLog)
UARTLogger.setLevel(LOG_LEVEL)
UARTLogger.addHandler(handler)

nameContactLog = "contactLogger"
filenameContactLog = timestr + "-contact.log"
fullpathContactLog = logfolderpath+filenameContactLog
contactlog_handler = logging.FileHandler(fullpathContactLog)
contact_formatter = logging.Formatter('%(message)s')
contactlog_handler.setFormatter(contact_formatter)
contactLogger = logging.getLogger(nameContactLog)
contactLogger.setLevel(LOG_LEVEL)
contactLogger.addHandler(contactlog_handler)

nameErrorLog = "errorLogger"
filenameErrorLog = timestr + "-errorLogger.log"
fullpathErrorLog = logfolderpath+filenameErrorLog
errorlog_handler = logging.FileHandler(fullpathErrorLog)
errorlog_formatter= logging.Formatter('%(message)s')
errorlog_handler.setFormatter(errorlog_formatter)
errorLogger = logging.getLogger(nameErrorLog)
errorLogger.setLevel(LOG_LEVEL)
errorLogger.addHandler(errorlog_handler)

class Network(object):

    def __init__(self):
        self.__nodes = []
        threading.Timer(NETWORK_PERIODIC_CHECK_INTERVAL,self.__periodicNetworkCheck).start()
        self.__consoleBuffer = deque()
        self.console_queue_lock = threading.Lock()
        self.__lastNetworkPrint=float(time.time())
        self.__netMaxTrickle=0
        self.__netMinTrickle=MAX_TRICKLE_C_VALUE
        self.__expTrickle=0
        self.__uartErrors=0
        self.__trickleQueue = deque()
        self.showHelp=False

    def getNode(self, label):
        for n in self.__nodes:
            if n.name == label:
                return n

        return None

    def addNode(self,n):
        self.__nodes.append(n)
        if len(self.__nodes) == 1:
            self.__expTrickle=n.lastTrickleCount

    def removeNode(self,label):
        n = self.getNode(label)
        if n != None:
            self.__nodes.remove(n)

    def removeNode(self,n):
        self.__nodes.remove(n)

    def __trickleCheck(self):
        for n in self.__nodes:
            #with n.lock:
            #self.addPrint("Node "+ n.name + "lastTrickleCount: " + str(n.lastTrickleCount))
            if n==self.__nodes[0]:
                self.__netMaxTrickle = n.lastTrickleCount
                self.__netMinTrickle = n.lastTrickleCount
            if n.lastTrickleCount > self.__netMaxTrickle or ( n.lastTrickleCount == 0 and self.__netMaxTrickle>=(MAX_TRICKLE_C_VALUE-1) ):
                self.__netMaxTrickle = n.lastTrickleCount
            if n.lastTrickleCount < self.__netMinTrickle or ( n.lastTrickleCount >= (MAX_TRICKLE_C_VALUE-1) and self.__netMinTrickle==0 ):
                self.__netMinTrickle = n.lastTrickleCount #todo: it seems that this does't work. The __netMinTrickle goes up before all the nodes have sent their new values, sometimes it is __netMaxTrickle that doesn't update as soon the first new value arrives..

        if self.__netMinTrickle == self.__netMaxTrickle and self.__netMaxTrickle == self.__expTrickle:
            return True
        else:
            return False

    def sendNewTrickle(self, message,forced=False):
        if forced:
            self.__trickleQueue.clear()
            send_serial_msg(message)
            net.addPrint("[APPLICATION] Trickle message: 0x {0} force send".format((message).hex()))
        else:
            if self.__trickleCheck():
                self.__expTrickle = (self.__netMaxTrickle + 1)%MAX_TRICKLE_C_VALUE
                send_serial_msg(message)
                net.addPrint("[APPLICATION] Trickle message: 0x {0} sent".format((message).hex()))
            else:
                self.__trickleQueue.append(message)
                self.addPrint("[APPLICATION] Trickle message: 0x {0} queued".format((message).hex()))

    def __periodicNetworkCheck(self):
        threading.Timer(NETWORK_PERIODIC_CHECK_INTERVAL,self.__periodicNetworkCheck).start()
        nodes_removed = False;
        for n in self.__nodes:
            if n.getLastMessageElapsedTime() > NODE_TIMEOUT_S and n.online:
                if printVerbosity > 2:
                    self.addPrint("[APPLICATION] Node "+ n.name +" timed out. Elasped time: %.2f" %n.getLastMessageElapsedTime() +" Removing it from the network.")
                #self.removeNode(n)
                n.online=False                
                nodes_removed = True

        #self.__trickleCheck()

        if nodes_removed:
            self.__trickleCheck()
            self.printNetworkStatus()

    def printNetworkStatus(self):

        if(float(time.time()) - self.__lastNetworkPrint < 0.2): #this is to avoid too fast call to this function
            return

        __lastNetworkPrint = float(time.time())

        netSize = len(self.__nodes)

        if CLEAR_CONSOLE:
            cls()

        print("|------------------------------------------------------------------------------|")
        print("|--------------------------|  Network size %3s errors: %4s |------------------|" %(str(netSize), self.__uartErrors))
        print("|------------| Trickle: min %3d; max %3d; exp %3d; queue size %2d |-------------|" %(self.__netMinTrickle, self.__netMaxTrickle, self.__expTrickle, len(self.__trickleQueue)))
        print("|------------------------------------------------------------------------------|")
        print("| NodeID | Battery                                | Last     | Trick   |  #BT  |")
        print("|        | Volt   SoC Capacty    Cons    Temp     | seen[s]  | Count   |  Rep  |")
        #print("|           |                 |                     |             |            |")
        for n in self.__nodes:
            n.printNodeInfo()
        #print("|           |                 |                     |             |            |")
        print("|------------------------------------------------------------------------------|")
        if self.showHelp:
            print("|    AVAILABLE COMMANDS:")
            print("| key    command\n|")
            print("| 1      request ping\n"
                  "| 2      enable bluetooth\n"
                  "| 3      disable bluetooth\n"
                  "| 4      bt_def\n"
                  "| 5      bt_with_params\n"
                  "| 6      enable battery info\n"
                  "| 7      disable battery info\n"
                  "| 8      reset nordic\n"
                  "| 9      set time between sends\n"
                  "| >9     set keep alive interval in seconds")
        else:
            print("|     h+enter    : Show available commands                                     |")
        print("|------------------------------------------------------------------------------|")
        print("|------------------|            CONSOLE                     |------------------|")
        print("|------------------------------------------------------------------------------|")

        terminalSize = shutil.get_terminal_size(([80,20]))
        if net.showHelp:
            availableLines = terminalSize[1] - (24 + len(self.__nodes))
        else:
            availableLines = terminalSize[1] - (12 + len(self.__nodes))

        while( (len(self.__consoleBuffer) > availableLines) and self.__consoleBuffer):
            with self.console_queue_lock:
                self.__consoleBuffer.popleft()

        with self.console_queue_lock:
            for l in self.__consoleBuffer:
                print(l)

    def processKeepAliveMessage(self, label, trickleCount, batteryVoltage, capacity):
        n = self.getNode(label)
        if n != None:
            n.updateTrickleCount(trickleCount)
            n.updateBatteryVoltage(batteryVoltage)
            n.updateBatteryCapacity(capacity)
            n.online=True 
        else:
            n=Node(label, trickleCount)
            n.updateBatteryVoltage(batteryVoltage)
            n.updateBatteryCapacity(capacity)
            self.addNode(n)
            

        if len(self.__trickleQueue) != 0:
            if self.__trickleCheck():
                self.__expTrickle = (self.__netMaxTrickle + 1)%MAX_TRICKLE_C_VALUE
                message=self.__trickleQueue.popleft()
                send_serial_msg(message)
                self.addPrint("[APPLICATION] Trickle message: 0x {0} automatically sent".format((message).hex()))
        else:
            self.__trickleCheck()
        self.printNetworkStatus()

    def processBatteryDataMessage(self, label, voltage, capacity, soc=None, consumption=None, temperature=None):
        n = self.getNode(label)
        if n != None:
            n.updateBatteryVoltage(voltage)
            n.updateBatteryCapacity(capacity)
            n.updateBatterySOC(soc)
            n.updateBatteryConsumption(consumption)
            n.updateBatteryTemperature(temperature)

        #else:
        #    n=Node(label, 0)
        #    self.addNode(n)
        #    n.updateBatteryVoltage(batteryVoltage)

    def processBTReportMessage(self, label):
        n = self.getNode(label)
        if n != None:
            n.BTReportHandler()
        #else:
        #    n=Node(label, 0)
        #    self.addNode(n)
        #    n.BTReportHandler()

    def processUARTError(self):
        self.__uartErrors=self.__uartErrors+1

    def addPrint(self, text):
        consoleLogger.info(text)
        terminalSize = shutil.get_terminal_size(([80,20]))
        if(len(text) > terminalSize[0]):
            if(len(text) > 2*terminalSize[0]):  #clip the text if it is longer than 2 lines...
                text=text[:2*terminalSize[0]]
            with self.console_queue_lock:
                self.__consoleBuffer.append(text[:terminalSize[0]])
                self.__consoleBuffer.append(text[terminalSize[0]:])
        else:
            with self.console_queue_lock:
                self.__consoleBuffer.append(text)

        self.printNetworkStatus()
        #for l in self.__consoleBuffer:
        #    print(l)

    def resetCounters(self):
        for n in self.__nodes:
            n.amountOfBTReports=0
        self.__trickleCheck()

    def resetTrickle(self):
        self.__trickleCheck()
        self.__expTrickle = self.__netMaxTrickle
        

class Node(object):

    # The class "constructor" - It's actually an initializer
    def __init__(self, label):
        self.lock = threading.Lock()
        self.name = label
        self.lastTrickleCount = 0
        self.lastMessageTime = float(time.time())
        self.online=True

    def __init__(self, label, trickleCount):
        self.lock = threading.Lock()
        self.name = label
        self.lastTrickleCount = trickleCount
        self.lastMessageTime = float(time.time())
        self.batteryVoltage = None
        self.batteryCapacity = None
        self.batterySoc = None
        self.batteryConsumption = None
        self.batteryTemperature = None
        self.amountOfBTReports = 0
        self.online=True

    def updateTrickleCount(self,trickleCount):
        with self.lock:
            self.lastTrickleCount = trickleCount
            self.lastMessageTime = float(time.time())

    def updateBatteryVoltage(self, batteryVoltage):
        with self.lock:
            self.batteryVoltage = batteryVoltage
            self.lastMessageTime = float(time.time())

    def updateBatteryCapacity(self, capacity):
        with self.lock:
            self.batteryCapacity = capacity
            self.lastMessageTime = float(time.time())

    def updateBatterySOC(self, soc):
        with self.lock:
            self.batterySoc = soc
            self.lastMessageTime = float(time.time())

    def updateBatteryConsumption(self, consumption):
        with self.lock:
            self.batteryConsumption = consumption
            self.lastMessageTime = float(time.time())

    def updateBatteryTemperature(self, temperature):
        with self.lock:
            self.batteryTemperature = temperature
            self.lastMessageTime = float(time.time())

    def BTReportHandler(self):
        with self.lock:
            self.amountOfBTReports = self.amountOfBTReports + 1
            self.lastMessageTime = float(time.time())

    def getLastMessageElapsedTime(self):
        now = float(time.time())
        return now-self.lastMessageTime

    def printNodeInfo(self):
        if self.online:
            if self.batteryConsumption != None:
                print("| %3s    | %3.2fV %3.0f%% %4.0fmAh %6.1fmA  %5.1f°    |  %3.0f     |  %3d    |%5d  |" % (str(self.name), self.batteryVoltage, self.batterySoc, self.batteryCapacity, self.batteryConsumption, self.batteryTemperature, self.getLastMessageElapsedTime(), self.lastTrickleCount, self.amountOfBTReports))
            else:
                print("| %3s    | %3.2fV                                  |  %3.0f     |  %3d    |%5d  |" % (str(self.name), self.batteryVoltage, self.getLastMessageElapsedTime(), self.lastTrickleCount, self.amountOfBTReports))
        else:
            if self.batteryConsumption != None:
                print("| %3s *  | %3.2fV %3.0f%% %4.0fmAh %6.1fmA  %5.1f°    |  %3.0f     |  %3d    |%5d  |" % (str(self.name), self.batteryVoltage, self.batterySoc, self.batteryCapacity, self.batteryConsumption, self.batteryTemperature, self.getLastMessageElapsedTime(), self.lastTrickleCount, self.amountOfBTReports))
            else:
                print("| %3s *  | %3.2fV                                  |  %3.0f     |  %3d    |%5d  |" % (str(self.name), self.batteryVoltage, self.getLastMessageElapsedTime(), self.lastTrickleCount, self.amountOfBTReports))


@unique
class PacketType(IntEnum):
    network_new_sequence =          0x0100
    network_active_sequence =       0x0101
    network_last_sequence =         0x0102
    network_bat_data =              0x0200
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
    ti_set_batt_info_int =          0xF026
    nordic_reset =                  0xF027
    nordic_ble_tof_enable =         0xF030
    ti_set_keep_alive =             0xF801

def cls():
    os.system('cls' if os.name=='nt' else 'clear')

def handle_user_input():
    ble_tof_enabled=False
    while 1:
        try:
            input_str = input()
            if len(input_str)>=2 and input_str[0] == 'f':
                forced=True
                user_input = int(input_str[1:])
            else:
                if input_str=='h':
                    if net.showHelp:
                        net.showHelp=False
                    else:
                        net.showHelp=True

                    net.printNetworkStatus()
                    continue
                elif input_str=='r':
                    net.resetCounters()
                    net.printNetworkStatus()
                    continue
                elif input_str=='t':
                    net.resetTrickle()
                    net.printNetworkStatus()
                    continue
                else:
                    forced=False
                    user_input = int(input_str)

            if user_input == 1:
                payload = 233
                net.addPrint("[USER_INPUT] Ping request")
                net.sendNewTrickle(build_outgoing_serial_message(PacketType.network_request_ping, payload.to_bytes(1, byteorder="big", signed=False)),forced)
            elif user_input == 2:
                net.addPrint("[USER_INPUT] Turn bluetooth on")
                net.sendNewTrickle(build_outgoing_serial_message(PacketType.nordic_turn_bt_on, None),forced)
            elif user_input == 3:
                net.addPrint("[USER_INPUT] Turn bluetooth off")
                net.sendNewTrickle(build_outgoing_serial_message(PacketType.nordic_turn_bt_off, None),forced)
            #elif user_input == 4:
            #    net.addPrint("Turning bt on low")
            #    appLogger.debug("[SENDING] Enable Bluetooth LOW")
            #    net.sendNewTrickle(build_outgoing_serial_message(PacketType.nordic_turn_bt_on_low, None),forced)
            #elif user_input == 4:
            #    net.addPrint("[USER_INPUT] Turn bluetooth on with default settings stored on the nodes")
            #    net.sendNewTrickle(build_outgoing_serial_message(PacketType.nordic_turn_bt_on_def, None),forced)
            #elif user_input == 6:
            #    net.addPrint("Turning bt on high")
            #    appLogger.debug("[SENDING] Enable Bluetooth HIGH")
            #    net.sendNewTrickle(build_outgoing_serial_message(PacketType.nordic_turn_bt_on_high, None),forced)
            elif user_input == 4:
                if ble_tof_enabled:
                    net.addPrint("[USER_INPUT] disabling ble tof")
                    ble_tof_enabled=False
                else:
                    net.addPrint("[USER_INPUT] enabling ble tof")
                    ble_tof_enabled=True
                
                net.sendNewTrickle(build_outgoing_serial_message(PacketType.nordic_ble_tof_enable, ble_tof_enabled.to_bytes(1, byteorder="big", signed=False)),forced)
                    
            elif user_input == 5:
                SCAN_INTERVAL_MS = 1500
                SCAN_WINDOW_MS = 1500
                SCAN_TIMEOUT_S = 0
                REPORT_TIMEOUT_S = 15

                active_scan = 1
                scan_interval = int(SCAN_INTERVAL_MS*1000/625)
                scan_window = int(SCAN_WINDOW_MS*1000/625)
                timeout = int(SCAN_TIMEOUT_S)
                report_timeout_ms = int(REPORT_TIMEOUT_S*1000)
                bactive_scan = active_scan.to_bytes(1, byteorder="big", signed=False)
                bscan_interval = scan_interval.to_bytes(2, byteorder="big", signed=False)
                bscan_window = scan_window.to_bytes(2, byteorder="big", signed=False)
                btimeout = timeout.to_bytes(2, byteorder="big", signed=False)
                breport_timeout_ms = report_timeout_ms.to_bytes(4, byteorder="big", signed=False)
                payload = bactive_scan + bscan_interval + bscan_window + btimeout + breport_timeout_ms
                net.addPrint("[USER_INPUT] Turn bluetooth on with parameters: scan_int="+str(SCAN_INTERVAL_MS)+"ms, scan_win="+str(SCAN_WINDOW_MS)+"ms, timeout="+str(SCAN_TIMEOUT_S)+"s, report_int="+str(REPORT_TIMEOUT_S)+"s")
                net.sendNewTrickle(build_outgoing_serial_message(PacketType.nordic_turn_bt_on_w_params, payload),forced)
            elif user_input == 6:
                bat_info_interval_s = 90
                net.addPrint("[USER_INPUT] Enable battery info with interval: "+str(bat_info_interval_s))
                net.sendNewTrickle(build_outgoing_serial_message(PacketType.ti_set_batt_info_int, bat_info_interval_s.to_bytes(1, byteorder="big", signed=False)),forced)
            elif user_input == 7:
                bat_info_interval_s = 0
                net.addPrint("[USER_INPUT] Disable battery info")
                net.sendNewTrickle(build_outgoing_serial_message(PacketType.ti_set_batt_info_int, bat_info_interval_s.to_bytes(1, byteorder="big", signed=False)),forced)
            elif user_input == 8:
                net.addPrint("[USER_INPUT] Reset nordic")
                net.sendNewTrickle(build_outgoing_serial_message(PacketType.nordic_reset, None),forced)
            elif user_input == 9:
                time_between_send_ms = 0
                time_between_send = time_between_send_ms.to_bytes(2, byteorder="big", signed=False)
                net.addPrint("[USER_INPUT] Set time between sends to "+ str(time_between_send_ms) + "ms")
                net.sendNewTrickle(build_outgoing_serial_message(PacketType.network_set_time_between_send, time_between_send),forced)
            elif user_input > 9:
                interval = user_input
                net.addPrint("[USER_INPUT] Set keep alive interval to "+ str(interval) + "s")
                net.sendNewTrickle(build_outgoing_serial_message(PacketType.ti_set_keep_alive, interval.to_bytes(1, byteorder="big", signed=False)),forced)
        except ValueError:
            net.addPrint("[USER_INPUT] Read failed. Read data: "+ input_str)

def build_outgoing_serial_message(pkttype, ser_payload):
    payload_size = 0

    if ser_payload is not None:
        payload_size = len(ser_payload)

    packet = payload_size.to_bytes(length=1, byteorder='big', signed=False) + pkttype.to_bytes(length=2, byteorder='big', signed=False)

    if ser_payload is not None:
        packet=packet+ser_payload

    ascii_packet=''.join('%02X'%i for i in packet)

    return ascii_packet.encode('utf-8')

def send_serial_msg(message):

    line = message + SER_END_CHAR
    ser.write(line)
    UARTLogger.info('[GATEWAY] ' + str(line))

def decode_payload(payload, seqid, size, packettype, pktnum): #TODO: packettype can be removed
    #raw_data = "Node {0} ".format(messageSequenceList[seqid].nodeid) + "0x {:04X} ".format(packettype) + "0x {:02X}".format(pktnum) + " 0x "
    cur=0
    try:
        for x in range(round(size / SINGLE_NODE_REPORT_SIZE)):
            nid = int(payload[cur:cur+12], 16) #int(ser.read(6).hex(), 16)
            cur=cur+12
            lastrssi = int(payload[cur:cur+2], 16) #int(ser.read(1).hex(), 16)
            cur=cur+2
            maxrssi = int(payload[cur:cur+2], 16) #int(ser.read(1).hex(), 16)
            cur=cur+2
            pktcounter = int(payload[cur:cur+2], 16) #int(ser.read(1).hex(), 16)
            cur=cur+2
            #net.addPrint("id = {:012X}".format(nid, 8))
            contact = ContactData(nid, lastrssi, maxrssi, pktcounter)
            messageSequenceList[seqid].datalist.append(contact)
            messageSequenceList[seqid].datacounter += SINGLE_NODE_REPORT_SIZE
            #raw_data += "{:012X}".format(nid, 8) + '{:02X}'.format(lastrssi) + '{:02X}'.format(maxrssi) + '{:02X}'.format(pktcounter)

    except ValueError:
        net.addPrint("[Node {0}] Requested to decode more bytes than available. Requested: {1}"
                          .format(messageSequenceList[seqid].nodeid, size))
        #appLogger.warning("[Node {0}] Requested to decode more bytes than available. Requested: {1}"
        #                  .format(messageSequenceList[seqid].nodeid, size))
    finally:
        #dataLogger.info(raw_data)
        return


def log_contact_data(seqid):
    seq = messageSequenceList[seqid]
    timestamp = seq.timestamp
    source = seq.nodeid
    logstring = "{0} Node {1} ".format(timestamp, source)
    for x in range(len(seq.datalist)):
        logstring += "{:012X}".format(seq.datalist[x].nodeid) + '{:02X}'.format(seq.datalist[x].lastRSSI) + '{:02X}'.format(seq.datalist[x].maxRSSI) + '{:02X}'.format(seq.datalist[x].pktCounter)

    contactLogger.info(logstring)

def get_sequence_index(nodeid):
    for x in range(len(messageSequenceList)):
        if messageSequenceList[x].nodeid == nodeid:
            return x
    return -1


def check_for_packetdrop(nodeid, pktnum):
    if pktnum == 0:
        return
    for x in range(len(nodeDropInfoList)):
        if nodeDropInfoList[x].nodeid == nodeid:
            global dropCounter
            dropCounter += pktnum - nodeDropInfoList[x].lastpkt - 1
            #appLogger.debug("[Node {0}] Dropped {1} packet(s). Latest packet: {2}, new packet: {3}"
            #                .format(nodeid, pktnum - nodeDropInfoList[x].lastpkt - 1, nodeDropInfoList[x].lastpkt,
            #                         pktnum))
            return
    nodeDropInfoList.append(NodeDropInfo(nodeid, pktnum))

def to_byte(hex_text):
    return binascii.unhexlify(hex_text)

_thread.start_new_thread(handle_user_input, ())
net = Network()

net.addPrint("[APPLICATION] Starting...")
net.addPrint("[APPLICATION] Logs are in: %s" %logfolderpath)

if ser.is_open:
    net.addPrint("[UART] Serial Port already open! "+ ser.port + " open before initialization... closing first")
    ser.close()
    time.sleep(1)

startCharErr=False
#startBuffErr=False
otherkindofErr=False

try:
    while 1:
        if ser.is_open:
            try:
                bytesWaiting = ser.in_waiting
            except Exception as e:
                net.addPrint("[UART] Serial Port input exception:"+ str(e))
                #appLogger.error("Serial Port input exception: {0}".format(e))
                bytesWaiting = 0
                ser.close()
                time.sleep(1)
                continue

            try:

                if bytesWaiting > 0:
                    rawline = ser.readline() #can block if no timeout is provided when the port is open
                    UARTLogger.info('[SINK] ' + str(rawline))

                    start = rawline[0:1].hex()
                    if start == START_CHAR:
                        if startCharErr or otherkindofErr:
                            net.processUARTError()
                            startCharErr=False
                            otherkindofErr=False

                        line = to_byte(rawline[1:-1])
                        cursor=0
                        deliverCounter += 1
                        # TODO change the way pktnum's are being tracked
                        nodeid = int.from_bytes(line[cursor:cursor+1], "little", signed=False) #int.from_bytes(ser.read(1), "little", signed=False)
                        cursor+=1
                        pkttype = int(line[cursor:cursor+2].hex(),16) #int(ser.read(2).hex(), 16)
                        cursor+=2
                        pktnum = int.from_bytes(line[cursor:cursor+1], "little", signed=False) #int.from_bytes(ser.read(1), "little", signed=False)
                        cursor+=1

                        if printVerbosity > 0:
                            net.addPrint("[NODEID " + str(nodeid) + "] pkttype " + hex(pkttype) + ", pktnum " + str(pktnum))
                        #appLogger.info("[New message] nodeid {0}, pkttype {1} pktnum {2}".format(nodeid, hex(pkttype), pktnum))

                        check_for_packetdrop(nodeid, pktnum)
                        #TODO: add here the check for duplicates. Any duplicate should be discarded HERE!!!

                        if PacketType.network_new_sequence == pkttype:
                            datalen = int(line[cursor:cursor+2].hex(), 16) #int(ser.read(2).hex(), 16)
                            cursor+=2
                            payload = line[cursor:].hex()
                            cursor = len(line)
                            if datalen % SINGLE_NODE_REPORT_SIZE != 0:
                                if printVerbosity > 1:
                                    net.addPrint("  [PACKET DECODE] Invalid datalength: "+ str(datalen))
                                #appLogger.warning("[Node {0}] invalid sequencelength: {1}".format(nodeid, datalen))
                            seqid = get_sequence_index(nodeid)
                            #at this point, since we just received 'network_new_sequence', there should not be any sequence associated with this nodeid
                            if seqid != -1:
                                if messageSequenceList[seqid].lastPktnum == pktnum: #in this case we just received the 'network_new_sequence' packet twice, shall I discard duplicates before this?
                                    if printVerbosity > 1:
                                        net.addPrint("  [PACKET DECODE] Duplicate packet from node "+ str(nodeid)+ " with pktnum "+ str(pktnum))
                                    #appLogger.info("[Node {0}] duplicate packet, pktnum: {1}".format(nodeid, pktnum))
                                    # remove duplicate packet data from uart buffer
                                    #remainingDataSize = messageSequenceList[seqid].sequenceSize - messageSequenceList[seqid].datacounter
                                    #if remainingDataSize >= MAX_PACKET_PAYLOAD:
                                    #    payload=line[10:10+MAX_PACKET_PAYLOAD].hex()
                                    #else:
                                    #    payload=line[10:10+remainingDataSize].hex()
                                    #dataLogger.info("Node {0} 0x {1} 0x {2} 0x {3}".format(nodeid, '{:04x}'.format(pkttype), '{:02x}'.format(pktnum),
                                    #                                              '{:x}'.format(int(payload, 16))))
                                    continue
                                else:   #in this case the 'network_new_sequence' arrived before 'network_last_sequence'.
                                    if printVerbosity > 1:
                                        net.addPrint("  [PACKET DECODE] Previous sequence has not been completed yet")
                                    # TODO what to do now? For now, assume last packet was dropped
                                    # TODO send received data instead of deleting it all
                                    #appLogger.info("[Node {0}] Received new sequence packet "
                                    #               "while old sequence has not been completed".format(nodeid))
                                    log_contact_data(seqid)
                                    del messageSequenceList[seqid]

                            messageSequenceList.append(MessageSequence(datetime.datetime.fromtimestamp(time.time()).strftime('%Y-%m-%d %H:%M:%S'),
                                                                       nodeid, pktnum, 0, 0, [], time.time()))
                            seqid = len(messageSequenceList) - 1
                            messageSequenceList[seqid].sequenceSize += datalen

                            if messageSequenceList[seqid].sequenceSize > MAX_PACKET_PAYLOAD: #this is the normal behaviour
                                decode_payload(payload, seqid, MAX_PACKET_PAYLOAD, PacketType.network_new_sequence, pktnum)
                            else:   #this is when the sequence is made by only one packet.
                                decode_payload(payload, seqid, datalen, PacketType.network_new_sequence, pktnum)

                                # TODO Only 1 packet in this sequence, so upload this packet already
                                if printVerbosity > 1:
                                    net.addPrint("  [PACKET DECODE] Bluetooth sequence decoded. Contact elements: "+ str(len(messageSequenceList[seqid].datalist)))
                                #appLogger.debug("[Node {0}] Single packet sequence complete".format(nodeid))
                                log_contact_data(seqid)
                                net.processBTReportMessage(str(nodeid))

                                del messageSequenceList[seqid]

                        elif PacketType.network_active_sequence == pkttype:
                            seqid = get_sequence_index(nodeid)
                            payload = line[cursor:].hex()
                            cursor = len(line)
                            if seqid == -1: #this is when we received 'network_active_sequence' before receiving a valid 'network_new_sequence'
                                if printVerbosity > 1:
                                    net.addPrint("  [PACKET DECODE] First part of sequence dropped, creating incomplete sequence at index "+ str(len(messageSequenceList)) +" from pktnum "+ str(pktnum))
                                messageSequenceList.append(
                                    MessageSequence(datetime.datetime.fromtimestamp(time.time()).
                                                    strftime('%Y-%m-%d %H:%M:%S'), nodeid,
                                                    pktnum, -1, 0, [], time.time()))
                                seqid = len(messageSequenceList) - 1
                                headerDropCounter += 1

                            elif messageSequenceList[seqid].lastPktnum == pktnum: #in this case we just received the same 'network_active_sequence' packet twice,
                                if printVerbosity > 1:
                                    net.addPrint("  [PACKET DECODE] Duplicate packet from node "+ str(nodeid) +" with pktnum "+ str(pktnum))
                                # remove duplicate packet data from uart buffer
                                #remainingDataSize = messageSequenceList[seqid].sequenceSize - messageSequenceList[seqid].datacounter
                                #if remainingDataSize >= MAX_PACKET_PAYLOAD:
                                    #ser.read(MAX_PACKET_PAYLOAD) #TODO:if it is a duplicate no need to store it, but it MUST be a duplicate
                                #else:
                                    #ser.read(remainingDataSize)
                                continue

                            messageSequenceList[seqid].lastPktnum = pktnum
                            messageSequenceList[seqid].latestTime = time.time()
                            decode_payload(payload,seqid, MAX_PACKET_PAYLOAD, PacketType.network_active_sequence, pktnum)

                        elif PacketType.network_last_sequence == pkttype:
                            # TODO upload data before deleting element from list
                            #if printVerbosity > 1:
                            #    net.addPrint("[INFO] Full message received")
                            seqid = get_sequence_index(nodeid)
                            payload = line[cursor:].hex()
                            cursor = len(line)
                            if seqid == -1: #this is when we receive 'network_last_sequence' before receiving a valid 'network_new_sequence'
                                messageSequenceList.append(MessageSequence(datetime.datetime.fromtimestamp(time.time()).strftime('%Y-%m-%d %H:%M:%S'), nodeid,
                                                                           pktnum, 0, 0, [], time.time()))
                                seqid = len(messageSequenceList) - 1

                                decode_payload(payload,seqid, MAX_PACKET_PAYLOAD, PacketType.network_last_sequence, pktnum)
                                if printVerbosity > 1:
                                    net.addPrint("  [PACKET DECODE] Bluetooth sequence decoded but header files were never received.  datacounter: "+ str(messageSequenceList[seqid].datacounter) +" ContactData elements: "+ str(len(messageSequenceList[seqid].datalist)))
                                    # net.addPrint("[INFO] Nodeid: "+ str(messageSequenceList[seqid].nodeid) +" datacounter: "+
                                    #       str(messageSequenceList[seqid].datacounter)+ " ContactData elements: "+
                                    #       str(len(messageSequenceList[seqid].datalist)))
                                headerDropCounter += 1
                                #appLogger.info("[Node {0}] Message defragmented but header files were never received"
                                #                " -  datacounter: {1} ContactData elements: {2}"
                                #               .format(nodeid, messageSequenceList[seqid].datacounter,
                                #                        len(messageSequenceList[seqid].datalist)))
                                log_contact_data(seqid)
                                del messageSequenceList[seqid]

                            elif messageSequenceList[seqid].sequenceSize == -1: #what is this???
                                decode_payload(payload,seqid, MAX_PACKET_PAYLOAD, PacketType.network_last_sequence, pktnum)
                                if printVerbosity > 1:
                                    net.addPrint("  [PACKET DECODE] Bluetooth sequence decoded but header files were never received.  datacounter: "+ str(messageSequenceList[seqid].datacounter) +" ContactData elements: "+ str(len(messageSequenceList[seqid].datalist)))
                                    #
                                    # net.addPrint("[WARNING] Message defragmented but header files were never received")
                                    # net.addPrint("[INFO] Nodeid: "+ str(messageSequenceList[seqid].nodeid) +" datacounter: "+ str(messageSequenceList[seqid].datacounter)+ " ContactData elements: "+ str(len(messageSequenceList[seqid].datalist)))
                                #appLogger.info("[Node {0}] Message defragmented but header files were never received"
                                #                " -  datacounter: {1} ContactData elements: "
                                #               .format(nodeid, messageSequenceList[seqid].datacounter,
                                #                        len(messageSequenceList[seqid].datalist)))
                                log_contact_data(seqid)
                                del messageSequenceList[seqid]

                            else:
                                remainingDataSize = messageSequenceList[seqid].sequenceSize - messageSequenceList[seqid].datacounter
                                if remainingDataSize > MAX_PACKET_PAYLOAD: #when is this supposed to happen???
                                    decode_payload(payload,seqid, MAX_PACKET_PAYLOAD, PacketType.network_last_sequence, pktnum)
                                else:
                                    decode_payload(payload,seqid, remainingDataSize, PacketType.network_last_sequence, pktnum)

                                if messageSequenceList[seqid].sequenceSize != messageSequenceList[seqid].datacounter:
                                    if printVerbosity > 1:
                                        #net.addPrint("ERROR: Messagesequence ended, but datacounter is not equal to sequencesize")
                                        net.addPrint("  [PACKET DECODE] ERROR: Messagesequence ended, but datacounter is not equal to sequencesize")

                                if printVerbosity > 1:
                                    net.addPrint("  [PACKET DECODE] Bluetooth sequence decoded. "+" sequencesize: "+ str(messageSequenceList[seqid].sequenceSize)+ " ContactData elements: "+ str(len(messageSequenceList[seqid].datalist)))
                                #appLogger.warning("[Node {0}] Messagesequence ended, but datacounter is not equal to sequencesize -  datacounter: {1}" #WHY IS THIS WARNING HERE?????
                                #                   " ContactData elements: {2}".format(nodeid, messageSequenceList[seqid].datacounter,
                                #                                                       len(messageSequenceList[seqid].datalist)))
                                log_contact_data(seqid)
                                net.processBTReportMessage(str(messageSequenceList[seqid].nodeid))
                                del messageSequenceList[seqid]

                        elif PacketType.network_bat_data == pkttype:
                            batCapacity = float(int.from_bytes(line[cursor:cursor+2], byteorder="big", signed=False)) #int.from_bytes(ser.read(2), byteorder="big", signed=False)
                            cursor+=2
                            batSoC = float(int.from_bytes(line[cursor:cursor+2], byteorder="big", signed=False)) / 10 #int.from_bytes(ser.read(2), byteorder="big", signed=False) / 10
                            cursor+=2
                            bytesELT = line[cursor:cursor+2] #ser.read(2)
                            cursor+=2
                            batELT = str(timedelta(minutes=int.from_bytes(bytesELT, byteorder="big", signed=False)))[:-3] # Convert minutes to hours and minutes
                            batAvgConsumption = float(int.from_bytes(line[cursor:cursor+2], byteorder="big", signed=True)) / 10 #int.from_bytes(ser.read(2), byteorder="big", signed=True) / 10
                            cursor+=2
                            batAvgVoltage = float(int.from_bytes(line[cursor:cursor+2], byteorder="big", signed=False))/1000 #int.from_bytes(ser.read(2), byteorder="big", signed=False)
                            cursor+=2
                            batAvgTemp = float(int.from_bytes(line[cursor:cursor+2], byteorder="big", signed=True)) / 100 #int.from_bytes(ser.read(2), byteorder="big", signed=True) / 100
                            cursor+=2
                            #processBatteryDataMessage(self, label, voltage, capacity, soc=None, consumption=None, temperature=None)
                            net.processBatteryDataMessage(str(nodeid), batAvgVoltage, batCapacity, batSoC, batAvgConsumption, batAvgTemp)

                            if printVerbosity > 1:
                                net.addPrint("  [PACKET DECODE] Battery data, Cap: %.0f mAh SoC: %.1f ETA: %s (hh:mm) Consumption: %.1f mA Voltage: %.3f Temperature %.2f"% (batCapacity, batSoC, batELT, batAvgConsumption, batAvgVoltage, batAvgTemp))

                            #appLogger.info("[Node {0}] Received bat data, Capacity: {1} mAh | State Of Charge: {2}% | Estimated Lifetime: {3} (hh:mm) | "
                            #                "Average Consumption: {4} mA | Average Battery Voltage: {5} mV | Temperature: {6} *C"
                            #               .format(nodeid, batCapacity, batSoC, batELT, batAvgConsumption,
                            #                        batAvgVoltage, batAvgTemp))
                            #dataLogger.info("Node {0} 0x {1} 0x {2} 0x {3}{4}{5}{6}{7}{8}".format(nodeid, '{:04x}'.format(pkttype), '{:02x}'.format(pktnum), '{:02x}'.format(batCapacity), '{:02x}'.format(int(batSoC * 10)),
                            #                                                                   '{:02x}'.format(int.from_bytes(bytesELT, byteorder="big", signed=False)), '{:02x}'.format(int(batAvgConsumption * 10)),
                            #                                                             '{:02x}'.format(int(batAvgVoltage)), '{:02x}'.format(100 * int(batAvgTemp))))

                        elif PacketType.network_respond_ping == pkttype:
                            payload = int(line[cursor:cursor+2].hex(), 16) #int(ser.read(1).hex(), 16)
                            cursor+=2
                            #dataLogger.info("Node {0} 0x {1} 0x {2} 0x {3}".format(nodeid, '{:04x}'.format(pkttype), '{:02x}'.format(pktnum), '{:x}'.format(payload)))
                            if payload == 233:
                                net.addPrint("  [PACKET DECODE] Node id "+ str(nodeid) +" pinged succesfully!")
                                #appLogger.debug("[Node {0}] pinged succesfully!".format(nodeid))
                                if nodeid not in ActiveNodes:
                                    ActiveNodes.append(nodeid)
                            else:
                                net.addPrint("  [PACKET DECODE] Node id "+ str(nodeid)+" wrong ping payload: %d" % payload )
                                #appLogger.info("[Node {0}] pinged wrong payload: ".format(nodeid, payload))

                        elif PacketType.network_keep_alive == pkttype:
                            cap = float(int.from_bytes(line[cursor:cursor+2], byteorder="big", signed=False)) #int.from_bytes(ser.read(2), byteorder="big", signed=False)
                            cursor+=2
                            batAvgVoltage = float(int.from_bytes(line[cursor:cursor+2], byteorder="big", signed=False))/1000 #int.from_bytes(ser.read(2), byteorder="big", signed=False)
                            cursor+=2
                            trickle_count = int.from_bytes(line[cursor:cursor+1], byteorder="big", signed=False) #int.from_bytes(ser.read(1), byteorder="big", signed=False)
                            cursor+=1

                            net.processKeepAliveMessage(str(nodeid), trickle_count, batAvgVoltage, cap)
                            if printVerbosity > 1:
                                net.addPrint("  [PACKET DECODE] Keep alive packet. Cap: "+ str(cap) +" Voltage: "+ str(batAvgVoltage*1000) +" Trickle count: "+ str(trickle_count))
                            #appLogger.info("[Node {0}] Received keep alive message with capacity: {1} and voltage: {2} trickle_count {3}".format(nodeid, cap, batAvgVoltage,trickle_count))
                            #dataLogger.info("Node {0} 0x {1} 0x {2} 0x {3}{4} {5}".format(nodeid, '{:02x}'.format(pkttype), '{:x}'.format(pktnum), '{:02x}'.format(cap), '{:02x}'.format(batAvgVoltage),trickle_count))

                        else:
                            net.addPrint("  [PACKET DECODE] Unknown packet (unrecognized packet type): "+str(rawline))
                            cursor = len(line)
                            #appLogger.warning("[Node {0}] Received unknown packettype: {1}".format(nodeid, hex(pkttype)))
                            #dataLogger.info("Node {0} 0x {1} 0x {2}".format(nodeid, '{:02x}'.format(pkttype), '{:x}'.format(pktnum)))

                    else:   #start == START_CHAR
                        startCharErr=True
                        net.addPrint("[UART] Unknown START_CHAR: "+ start + ". The line was: "+str(rawline))
                        errorLogger.info("%s" %(str(rawline)))

            except Exception as e:
                otherkindofErr=True
                net.addPrint("[ERROR] Unknown error during line decoding. Exception: %s. Line was: %s" %( str(e), str(rawline)))
                errorLogger.info("%s" %(str(rawline)))

            currentTime = time.time()
            if currentTime - previousTimeTimeout > timeoutInterval:
                previousTimeTimeout = time.time()
                deletedCounter = 0
                for x in range(len(messageSequenceList)):
                    if currentTime - messageSequenceList[x - deletedCounter].latestTime > timeoutTime:
                        deleted_nodeid=messageSequenceList[x - deletedCounter].nodeid
                        del messageSequenceList[x - deletedCounter]
                        deletedCounter += 1
                        if printVerbosity > 1:
                            xd = x + deletedCounter
                            net.addPrint("[APPLICATION] Deleted seqid %d of node %d because of timeout" %(xd, deleted_nodeid))

            if currentTime - btPreviousTime > btToggleInterval:
                ptype = 0
                if btToggleBool:
                    # net.addPrint("Turning bt off")
                    # appLogger.debug("[SENDING] Disable Bluetooth")
                    ptype = PacketType.nordic_turn_bt_off
                    # btToggleBool = False
                else:
                    # net.addPrint("Turning bt on")
                    # appLogger.debug("[SENDING] Enable Bluetooth")
                    ptype = PacketType.nordic_turn_bt_on
                    btToggleBool = True
                # send_serial_msg(ptype, None)
                btPreviousTime = currentTime


        else: # !ser.is_open (serial port is not open)

            net.addPrint('[UART] Serial Port closed! Trying to open port: %s'% ser.port)

            try:
                ser.open()
            except Exception as e:
                net.addPrint("[UART] Serial Port open exception:"+ str(e))
                #appLogger.debug("Serial Port exception: %s", e)
                time.sleep(5)
                continue

            net.addPrint("[UART] Serial Port open!")
            #appLogger.debug("Serial Port open")


except UnicodeDecodeError as e:
    pass

except KeyboardInterrupt:
    print("[APPLICATION] -----Packet delivery stats summary-----")
    print("[APPLICATION] Total packets delivered: ", deliverCounter)
    print("[APPLICATION] Total packets dropped: ", dropCounter)
    print("[APPLICATION] Total header packets dropped: ", headerDropCounter)
    print("[APPLICATION] Packet delivery rate: ", 100 * (deliverCounter / (deliverCounter + dropCounter)))
    print("[APPLICATION] Messages defragmented: ", defragmentationCounter)
    print("[APPLICATION] Logs are in: "+logfolderpath)

    #appLogger.info("-----Packet delivery stats summary-----")
    #appLogger.info("Total packets delivered: {0}".format(deliverCounter))
    #appLogger.info("Total packets dropped: {0}".format(dropCounter))
    #appLogger.info("Total header packets dropped: {0}".format(headerDropCounter))
    #appLogger.info("Packet delivery rate: {0}".format(100 * (deliverCounter / (deliverCounter + dropCounter))))
    #appLogger.info("Messages defragmented: {0}".format(defragmentationCounter))
    raise

finally:
    print("done")
