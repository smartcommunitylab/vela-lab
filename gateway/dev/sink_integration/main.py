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

START_CHAR = '2a'
START_BUF = '2a2a2a'  # this is not really a buffer
BAUD_RATE = 921600 #1000000
SERIAL_PORT = "/dev/ttyACM0" #"/dev/ttyS0"

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

SINGLE_REPORT_SIZE = 9
MAX_ONGOING_SEQUENCES = 10
MAX_PACKET_PAYLOAD = 54

MAX_BEACONS = 100

MAX_TRICKLE_C_VALUE = 256

ser = serial.Serial()
ser.port = SERIAL_PORT
ser.baudrate = BAUD_RATE
ser.timeout = 1

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

NODE_TIMEOUT_S = 120
NETWORK_PERIODIC_CHECK_INTERVAL = 2
CLEAR_CONSOLE = True

# Loggers
printVerbosity = 10

LOG_LEVEL = logging.DEBUG

formatter = logging.Formatter('%(asctime)s %(levelname)s %(message)s', datefmt='%Y/%m/%d %H:%M:%S')
timestr = time.strftime("%Y%m%d-%H%M%S")

nameAppLog = "appLogger"
filenameAppLog = timestr + "-app.log"
fullpathAppLog = logfolderpath+filenameAppLog
handler = logging.FileHandler(fullpathAppLog)
handler.setFormatter(formatter)
appLogger = logging.getLogger(nameAppLog)
appLogger.setLevel(LOG_LEVEL)
appLogger.addHandler(handler)

nameDataLog = "dataLogger"
filenameDataLog = timestr + "-data.log"
fullpathDataLog = logfolderpath+filenameDataLog
datalog_handler = logging.FileHandler(fullpathDataLog)
datalog_handler.setFormatter(formatter)
dataLogger = logging.getLogger(nameDataLog)
dataLogger.setLevel(LOG_LEVEL)
dataLogger.addHandler(datalog_handler)

nameContactLog = "contactLogger"
filenameContactLog = timestr + "-contact.log"
fullpathContactLog = logfolderpath+filenameContactLog
contactlog_handler = logging.FileHandler(fullpathContactLog)
contact_formatter = logging.Formatter('%(message)s')
contactlog_handler.setFormatter(contact_formatter)
contactLogger = logging.getLogger(nameContactLog)
contactLogger.setLevel(LOG_LEVEL)
contactLogger.addHandler(contactlog_handler)

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
        self.__trickleQueue = deque()

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
            self.addPrint("Trickle message: 0x {0} force send".format((message).hex()))
        else:
            if self.__trickleCheck():
                self.__expTrickle = (self.__netMaxTrickle + 1)%MAX_TRICKLE_C_VALUE
                send_serial_msg(message)
                self.addPrint("Trickle message: 0x {0} sent".format((message).hex()))
            else:
                self.__trickleQueue.append(message)
                self.addPrint("Trickle message: 0x {0} queued".format((message).hex()))

    def __periodicNetworkCheck(self):
        #net.addPrint("Periodic check...")
        threading.Timer(NETWORK_PERIODIC_CHECK_INTERVAL,self.__periodicNetworkCheck).start()
        nodes_removed = False;
        for n in self.__nodes:
            #self.addPrint("Node "+ n.name)
            if n.getLastMessageElapsedTime() > NODE_TIMEOUT_S:
                if printVerbosity > 2:
                    self.addPrint("Node "+ n.name +" timed out. Elasped time: %.2f" %n.getLastMessageElapsedTime() +" Removing it from the network.")
                self.removeNode(n)
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
        print("|------------------|            Network size %3s            |------------------|" %str(netSize))
        print("|------------| Trickle: min %3d; max %3d; exp %3d; queue size %2d |-------------|" %(self.__netMinTrickle, self.__netMaxTrickle, self.__expTrickle, len(self.__trickleQueue)))
        print("|------------------------------------------------------------------------------|")
        print("|  Node ID  |  Batt Volt [v]  |  Last seen [s] ago  |  Tkl Count  |  # BT rep  |")
        print("|           |                 |                     |             |            |")
        for n in self.__nodes:
            n.printNodeInfo()
        print("|           |                 |                     |             |            |")
        print("|------------------------------------------------------------------------------|\n|")
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
              "| >8     set keep alive interval in seconds\n|")
        print("|------------------------------------------------------------------------------|")
        print("|------------------|            CONSOLE                     |------------------|")
        print("|------------------------------------------------------------------------------|\n")

        terminalSize = shutil.get_terminal_size(([80,20]))

        while( len(self.__consoleBuffer) > (terminalSize[1] - (27 + len(self.__nodes))) and self.__consoleBuffer):
            with self.console_queue_lock:
                self.__consoleBuffer.popleft()

        with self.console_queue_lock:
            for l in self.__consoleBuffer:
                print(l)   

    def processKeepAliveMessage(self, label, trickleCount, batteryVoltage):
        n = self.getNode(label)
        if n != None:
            n.updateTrickleCount(trickleCount)
            n.updateBatteryVoltage(batteryVoltage)
        else:
            n=Node(label, trickleCount)
            n.updateBatteryVoltage(batteryVoltage)
            self.addNode(n)
        
        if len(self.__trickleQueue) != 0:
            if self.__trickleCheck():
                self.__expTrickle = (self.__netMaxTrickle + 1)%MAX_TRICKLE_C_VALUE
                message=self.__trickleQueue.popleft()
                send_serial_msg(message)
                self.addPrint("Trickle message: 0x {0} automatically sent".format((message).hex()))
        else:
            self.__trickleCheck()
        self.printNetworkStatus()

    def processBatteryDataMessage(self, label, batteryVoltage):
        n = self.getNode(label)
        if n != None:
            n.updateBatteryVoltage(batteryVoltage)
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

    def addPrint(self, text):        
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
    
class Node(object):
    
    # The class "constructor" - It's actually an initializer
    def __init__(self, label):
        self.lock = threading.Lock()
        self.name = label
        self.lastTrickleCount = 0
        self.lastMessageTime = float(time.time())
    
    def __init__(self, label, trickleCount):
        self.lock = threading.Lock()
        self.name = label
        self.lastTrickleCount = trickleCount
        self.lastMessageTime = float(time.time())
        self.batteryVoltage = -1
        self.amountOfBTReports = 0

    def updateTrickleCount(self,trickleCount):
        with self.lock:
            self.lastTrickleCount = trickleCount
            self.lastMessageTime = float(time.time())

    def updateBatteryVoltage(self, batteryVoltage):
        with self.lock:
            self.batteryVoltage = batteryVoltage
            self.lastMessageTime = float(time.time())

    def BTReportHandler(self):
        with self.lock:
            self.amountOfBTReports = self.amountOfBTReports + 1
            self.lastMessageTime = float(time.time())

    def getLastMessageElapsedTime(self):
        now = float(time.time())
        return now-self.lastMessageTime

    def printNodeInfo(self):
        if(self.batteryVoltage>0):
            print("|  %3s      |  %1.2f           |  %3.0f                |  %3d        |  %5d     |" % (str(self.name), self.batteryVoltage, self.getLastMessageElapsedTime(), self.lastTrickleCount, self.amountOfBTReports))
        else:
            print("|  %3s      |  %1.2f          |  %3.0f                |  %3d        |  %5d     |" % (str(self.name), self.batteryVoltage, self.getLastMessageElapsedTime(), self.lastTrickleCount, self.amountOfBTReports))
        
@unique
class PacketType(IntEnum):
    network_new_sequence =          0x0100
    network_active_sequence =       0x0101
    network_last_sequence =         0x0102
    network_bat_data =              0x0200
    network_request_ping =          0xF000
    network_respond_ping =          0xF001
    network_keep_alive =            0xF010
    ti_set_keep_alive =             0xF801
    nordic_turn_bt_off =            0xF020
    nordic_turn_bt_on =             0xF021
    nordic_turn_bt_on_w_params =    0xF022
    nordic_turn_bt_on_low =         0xF023  #deprecated
    nordic_turn_bt_on_def =         0xF024
    nordic_turn_bt_on_high =        0xF025  #deprecated
    ti_set_batt_info_int =          0xF026
    nordic_reset =                  0xF027

def cls():
    os.system('cls' if os.name=='nt' else 'clear')

def handle_user_input():
    while 1:
        try:
            input_str = input()
            #net.addPrint("user_input "+ input_str + " len "+ str(len(input_str)))
            if len(input_str)>=2 and input_str[0] == 'f':
                forced=True
                user_input = int(input_str[1:])
            else:
                forced=False
                user_input = int(input_str)

            if user_input == 1:
                payload = 233
                net.addPrint("Sent ping request")
                appLogger.debug("[SENDING] Requesting ping")
                net.sendNewTrickle(build_outgoing_serial_message(PacketType.network_request_ping, payload.to_bytes(1, byteorder="big", signed=False)),forced)
            elif user_input == 2:
                net.addPrint("Turning bt on")
                appLogger.debug("[SENDING] Enable Bluetooth")
                net.sendNewTrickle(build_outgoing_serial_message(PacketType.nordic_turn_bt_on, None),forced)
            elif user_input == 3:
                net.addPrint("Turning bt off")
                appLogger.debug("[SENDING] Disable Bluetooth")
                net.sendNewTrickle(build_outgoing_serial_message(PacketType.nordic_turn_bt_off, None),forced)
            #elif user_input == 4:
            #    net.addPrint("Turning bt on low")
            #    appLogger.debug("[SENDING] Enable Bluetooth LOW")
            #    net.sendNewTrickle(build_outgoing_serial_message(PacketType.nordic_turn_bt_on_low, None),forced)
            elif user_input == 4:
                net.addPrint("Turning bt on def")
                appLogger.debug("[SENDING] Enable Bluetooth DEF")
                net.sendNewTrickle(build_outgoing_serial_message(PacketType.nordic_turn_bt_on_def, None),forced)
            #elif user_input == 6:
            #    net.addPrint("Turning bt on high")
            #    appLogger.debug("[SENDING] Enable Bluetooth HIGH")
            #    net.sendNewTrickle(build_outgoing_serial_message(PacketType.nordic_turn_bt_on_high, None),forced)
            elif user_input == 5:
                SCAN_INTERVAL_MS = 5000
                SCAN_WINDOW_MS = 3500
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
                net.sendNewTrickle(build_outgoing_serial_message(PacketType.nordic_turn_bt_on_w_params, payload),forced)
                net.addPrint("Turn on bt with params")
                appLogger.debug("[SENDING] Enable Bluetooth with params")
            elif user_input == 6:
                bat_info_interval_s = 60
                net.addPrint("Enable battery info with interval: "+str(bat_info_interval_s))
                net.sendNewTrickle(build_outgoing_serial_message(PacketType.ti_set_batt_info_int, bat_info_interval_s.to_bytes(1, byteorder="big", signed=False)),forced)
            elif user_input == 7:
                bat_info_interval_s = 0
                net.addPrint("Disabling battery info")
                net.sendNewTrickle(build_outgoing_serial_message(PacketType.ti_set_batt_info_int, bat_info_interval_s.to_bytes(1, byteorder="big", signed=False)),forced)
            elif user_input == 8:
                net.addPrint("Reset bluetooth")
                net.sendNewTrickle(build_outgoing_serial_message(PacketType.nordic_reset, None),forced)
            elif user_input > 8:
                interval = user_input
                net.addPrint("Setting keep alive")
                net.sendNewTrickle(build_outgoing_serial_message(PacketType.ti_set_keep_alive, interval.to_bytes(1, byteorder="big", signed=False)),forced)
        except ValueError:
            net.addPrint("read failed")

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
    ser.write(message)
    ser.write(SER_END_CHAR)
    dataLogger.info("GATEWAY 0x {0} ".format((message+SER_END_CHAR).hex()))


def decode_payload(seqid, size, packettype, pktnum):
    raw_data = "Node {0} ".format(messageSequenceList[seqid].nodeid) + "0x {:04X} ".format(packettype) + "0x {:02X}".format(pktnum) + " 0x "
    try:
        for x in range(round(size / SINGLE_REPORT_SIZE)):
            nid = int(ser.read(6).hex(), 16)
            lastrssi = int(ser.read(1).hex(), 16)
            maxrssi = int(ser.read(1).hex(), 16)
            pktcounter = int(ser.read(1).hex(), 16)
            contact = ContactData(nid, lastrssi, maxrssi, pktcounter)
            messageSequenceList[seqid].datalist.append(contact)
            messageSequenceList[seqid].datacounter += SINGLE_REPORT_SIZE
            raw_data += "{:012X}".format(nid, 8) + '{:02X}'.format(lastrssi) + '{:02X}'.format(maxrssi) + '{:02X}'.format(pktcounter)
    except ValueError:
        net.addPrint("[DecodePayload] Payload size to decode is smaller than available bytes")
        appLogger.warning("[Node {0}] Requested to decode more bytes than available. Requested: {1}"
                          .format(messageSequenceList[seqid].nodeid, size))
    finally:
        dataLogger.info(raw_data)
        return


def log_contact_data(seqid):
    seq = messageSequenceList[seqid]
    timestamp = seq.timestamp
    source = seq.nodeid
    logstring = "{0} Node {1} ".format(timestamp, source)
    for x in range(len(seq.datalist)):
        logstring += "{:012X}".format(seq.datalist[x].nodeid) + '{:02X}'.format(seq.datalist[x].lastRSSI) + '{:02X}'.format(seq.datalist[x].maxRSSI) + '{:02X}'.format(seq.datalist[x].pktCounter)

    contactLogger.warning(logstring)

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
            appLogger.debug("[Node {0}] Dropped {1} packet(s). Latest packet: {2}, new packet: {3}"
                            .format(nodeid, pktnum - nodeDropInfoList[x].lastpkt - 1, nodeDropInfoList[x].lastpkt,
                                     pktnum))
            return
    nodeDropInfoList.append(NodeDropInfo(nodeid, pktnum))


_thread.start_new_thread(handle_user_input, ())
net = Network()

net.addPrint("Starting...")
net.addPrint("Logs are in: %s" %logfolderpath)

if ser.is_open:
    net.addPrint("Serial Port already open! "+ ser.port + " open before initialization... closing first")
    ser.close()
    time.sleep(1)

try:
    while 1:
        if ser.is_open:
            try:
                bytesWaiting = ser.in_waiting
            except Exception as e:
                net.addPrint("Serial Port input exception:"+ str(e))
                appLogger.error("Serial Port input exception: {0}".format(e))
                bytesWaiting = 0
                ser.close()
                time.sleep(1)
                continue

            if bytesWaiting > 0:
                start = ser.read(1).hex()
                if start == START_CHAR:
                    start = ser.read(3).hex()
                    if start == START_BUF:
                        deliverCounter += 1
                        # TODO change the way pktnum's are being tracked
                        nodeid = int.from_bytes(ser.read(1), "little", signed=False)
                        pkttype = int(ser.read(2).hex(), 16)
                        pktnum = int.from_bytes(ser.read(1), "little", signed=False)
                        if printVerbosity > 0:
                            net.addPrint("[New message] nodeid " + str(nodeid) + ", pkttype " + hex(pkttype) + ", pktnum " + str(pktnum))
                        appLogger.info("[New message] nodeid {0}, pkttype {1} pktnum {2}".format(nodeid, hex(pkttype), pktnum))
                        check_for_packetdrop(nodeid, pktnum)

                        if PacketType.network_new_sequence == pkttype:
                            datalen = int(ser.read(2).hex(), 16)
                            if datalen % SINGLE_REPORT_SIZE != 0:
                                if printVerbosity > 1:
                                    net.addPrint("Invalid datalength: "+ str(datalen))
                                appLogger.warning("[Node {0}] invalid sequencelength: {1}".format(nodeid, datalen))
                            seqid = get_sequence_index(nodeid)
                            if seqid != -1:
                                if messageSequenceList[seqid].lastPktnum == pktnum:
                                    if printVerbosity > 1:
                                        net.addPrint("Duplicate packet from node "+ str(nodeid)+ " with pktnum "+ str(pktnum))
                                    appLogger.info("[Node {0}] duplicate packet, pktnum: {1}".format(nodeid, pktnum))
                                    # remove duplicate packet data from uart buffer
                                    remainingDataSize = messageSequenceList[seqid].sequenceSize - messageSequenceList[seqid].datacounter
                                    if remainingDataSize >= MAX_PACKET_PAYLOAD:
                                        dataLogger.info("Node {0} 0x {1} 0x {2} 0x {3}".format(nodeid, '{:04x}'.format(pkttype), '{:02x}'.format(pktnum),
                                                                                      '{:x}'.format(int(ser.read(MAX_PACKET_PAYLOAD).hex(), 16))))
                                    else:
                                        dataLogger.info("Node {0} 0x {1} 0x {2} 0x {3}".format(nodeid, '{:04x}'.format(pkttype), '{:02x}'.format(pktnum),
                                                                                      '{:x}'.format(int(ser.read(remainingDataSize).hex(), 16))))
                                    continue
                                else:
                                    if printVerbosity > 1:
                                        net.addPrint("Previous sequence has not been completed yet")
                                    # TODO what to do now? For now, assume last packet was dropped
                                    # TODO send received data instead of deleting it all
                                    appLogger.info("[Node {0}] Received new sequence packet "
                                                   "while old sequence has not been completed".format(nodeid))
                                    log_contact_data(seqid)
                                    del messageSequenceList[seqid]

                            messageSequenceList.append(MessageSequence(datetime.datetime.fromtimestamp(time.time()).strftime('%Y-%m-%d %H:%M:%S'),
                                                                       nodeid, pktnum, 0, 0, [], time.time()))
                            seqid = len(messageSequenceList) - 1
                            messageSequenceList[seqid].sequenceSize += datalen

                            if messageSequenceList[seqid].sequenceSize > MAX_PACKET_PAYLOAD - SINGLE_REPORT_SIZE:
                                decode_payload(seqid, MAX_PACKET_PAYLOAD - SINGLE_REPORT_SIZE, PacketType.network_new_sequence, pktnum)

                            else:
                                decode_payload(seqid, datalen, PacketType.network_new_sequence, pktnum)

                                # TODO Only 1 packet in this sequence, so upload this packet already
                                if printVerbosity > 1:
                                    net.addPrint("Single packet sequence completed")
                                appLogger.debug("[Node {0}] Single packet sequence complete".format(nodeid))
                                log_contact_data(seqid)                                
                                net.processBTReportMessage(str(nodeid))

                                del messageSequenceList[seqid]

                        elif PacketType.network_active_sequence == pkttype:
                            seqid = get_sequence_index(nodeid)
                            if seqid == -1:
                                if printVerbosity > 1:
                                    net.addPrint("First part of sequence dropped, creating incomplete sequence at index "+ str(len(messageSequenceList)) +" from pktnum "+ str(pktnum))
                                messageSequenceList.append(
                                    MessageSequence(datetime.datetime.fromtimestamp(time.time()).
                                                    strftime('%Y-%m-%d %H:%M:%S'), nodeid,
                                                    pktnum, -1, 0, [], time.time()))
                                seqid = len(messageSequenceList) - 1
                                headerDropCounter += 1

                            elif messageSequenceList[seqid].lastPktnum == pktnum:
                                if printVerbosity > 1:
                                    net.addPrint("Duplicate packet from node "+ str(nodeid) +" with pktnum "+ str(pktnum))
                                # remove duplicate packet data from uart buffer
                                remainingDataSize = messageSequenceList[seqid].sequenceSize - messageSequenceList[seqid].datacounter
                                if remainingDataSize >= MAX_PACKET_PAYLOAD:
                                    ser.read(MAX_PACKET_PAYLOAD)
                                else:
                                    ser.read(remainingDataSize)
                                    continue

                            messageSequenceList[seqid].lastPktnum = pktnum
                            messageSequenceList[seqid].latestTime = time.time()
                            decode_payload(seqid, MAX_PACKET_PAYLOAD, PacketType.network_active_sequence, pktnum)

                        elif PacketType.network_last_sequence == pkttype:
                            # TODO upload data before deleting element from list                            
                            if printVerbosity > 1:
                                net.addPrint("Full message defragmented")
                            seqid = get_sequence_index(nodeid)
                            if seqid == -1:
                                messageSequenceList.append(MessageSequence(datetime.datetime.fromtimestamp(time.time()).strftime('%Y-%m-%d %H:%M:%S'), nodeid,
                                                                           pktnum, 0, 0, [], time.time()))
                                seqid = len(messageSequenceList) - 1
                                decode_payload(seqid, MAX_PACKET_PAYLOAD, PacketType.network_last_sequence, pktnum)
                                if printVerbosity > 1:
                                    net.addPrint("Message defragmented but header files were never received")
                                    net.addPrint("Nodeid: "+ str(messageSequenceList[seqid].nodeid) +" datacounter: "+
                                          str(messageSequenceList[seqid].datacounter)+ " ContactData elements: "+
                                          str(len(messageSequenceList[seqid].datalist)))
                                headerDropCounter += 1
                                appLogger.info("[Node {0}] Message defragmented but header files were never received"
                                                " -  datacounter: {1} ContactData elements: {2}"
                                               .format(nodeid, messageSequenceList[seqid].datacounter,
                                                        len(messageSequenceList[seqid].datalist)))
                                log_contact_data(seqid)
                                del messageSequenceList[seqid]

                            elif messageSequenceList[seqid].sequenceSize == -1:
                                decode_payload(seqid, MAX_PACKET_PAYLOAD, PacketType.network_last_sequence, pktnum)
                                if printVerbosity > 1:
                                    net.addPrint("Message defragmented but header files were never received")
                                    net.addPrint("Nodeid: "+ str(messageSequenceList[seqid].nodeid) +" datacounter: "+ str(messageSequenceList[seqid].datacounter)+ " ContactData elements: "+ str(len(messageSequenceList[seqid].datalist)))
                                appLogger.info("[Node {0}] Message defragmented but header files were never received"
                                                " -  datacounter: {1} ContactData elements: "
                                               .format(nodeid, messageSequenceList[seqid].datacounter,
                                                        len(messageSequenceList[seqid].datalist)))
                                log_contact_data(seqid)
                                del messageSequenceList[seqid]

                            else:
                                remainingDataSize = messageSequenceList[seqid].sequenceSize - messageSequenceList[seqid].datacounter
                                if remainingDataSize > MAX_PACKET_PAYLOAD:
                                    decode_payload(seqid, MAX_PACKET_PAYLOAD, PacketType.network_last_sequence, pktnum)
                                else:
                                    decode_payload(seqid, remainingDataSize, PacketType.network_last_sequence, pktnum)
                                if messageSequenceList[seqid].sequenceSize != messageSequenceList[seqid].datacounter:
                                    if printVerbosity > 1:
                                        net.addPrint("ERROR: Messagesequence ended, but datacounter is not equal to sequencesize")

                                if printVerbosity > 1:
                                    net.addPrint("Nodeid: "+ str(messageSequenceList[seqid].nodeid)+ " sequencesize: "+ str(messageSequenceList[seqid].sequenceSize)+ " ContactData elements: "+ str(len(messageSequenceList[seqid].datalist)))
                                appLogger.warning("[Node {0}] Messagesequence ended, but datacounter is not equal to sequencesize -  datacounter: {1}" #WHY IS THIS WARNING HERE?????
                                                   " ContactData elements: {2}".format(nodeid, messageSequenceList[seqid].datacounter,
                                                                                       len(messageSequenceList[seqid].datalist)))
                                log_contact_data(seqid)
                                net.processBTReportMessage(str(messageSequenceList[seqid].nodeid))
                                del messageSequenceList[seqid]

                        elif PacketType.network_bat_data == pkttype:
                            batCapacity = int.from_bytes(ser.read(2), byteorder="big", signed=False)
                            batSoC = int.from_bytes(ser.read(2), byteorder="big", signed=False) / 10
                            bytesELT = ser.read(2)
                            batELT = str(timedelta(minutes=int.from_bytes(bytesELT, byteorder="big", signed=False)))[:-3] # Convert minutes to hours and minutes
                            batAvgConsumption = int.from_bytes(ser.read(2), byteorder="big", signed=True) / 10
                            batAvgVoltage = int.from_bytes(ser.read(2), byteorder="big", signed=False)
                            batAvgTemp = int.from_bytes(ser.read(2), byteorder="big", signed=True) / 100

                            net.processBatteryDataMessage(str(nodeid), float(batAvgVoltage)/1000)                            
                            
                            if printVerbosity > 1:
                                net.addPrint("Received bat data, Capacity: %d mAh | State Of Charge: %.1f | Estimated Lifetime: %s (hh:mm) | "
                                      "Average Consumption: %.1f mA | Average Battery Voltage: %d | Average Temperature %.2f"
                                      % (batCapacity, batSoC, batELT, batAvgConsumption, batAvgVoltage, batAvgTemp))
                            appLogger.info("[Node {0}] Received bat data, Capacity: {1} mAh | State Of Charge: {2}% | Estimated Lifetime: {3} (hh:mm) | "
                                            "Average Consumption: {4} mA | Average Battery Voltage: {5} mV | Temperature: {6} *C"
                                           .format(nodeid, batCapacity, batSoC, batELT, batAvgConsumption,
                                                    batAvgVoltage, batAvgTemp))
                            dataLogger.info("Node {0} 0x {1} 0x {2} 0x {3}{4}{5}{6}{7}{8}".format(nodeid, '{:04x}'.format(pkttype), '{:02x}'.format(pktnum), '{:02x}'.format(batCapacity), '{:02x}'.format(int(batSoC * 10)),
                                                                                               '{:02x}'.format(int.from_bytes(bytesELT, byteorder="big", signed=False)), '{:02x}'.format(int(batAvgConsumption * 10)),
                                                                                         '{:02x}'.format(int(batAvgVoltage)), '{:02x}'.format(100 * int(batAvgTemp))))

                        elif PacketType.network_respond_ping == pkttype:
                            payload = int(ser.read(1).hex(), 16)
                            dataLogger.info("Node {0} 0x {1} 0x {2} 0x {3}".format(nodeid, '{:04x}'.format(pkttype), '{:02x}'.format(pktnum), '{:x}'.format(payload)))
                            if payload == 233:
                                net.addPrint("Node id "+ str(nodeid) +" pinged succesfully!")
                                appLogger.debug("[Node {0}] pinged succesfully!".format(nodeid))
                                if nodeid not in ActiveNodes:
                                    ActiveNodes.append(nodeid)
                            else:
                                net.addPrint("Wrong ping payload: %d" % payload )
                                appLogger.info("[Node {0}] pinged wrong payload: ".format(nodeid, payload))

                        elif PacketType.network_keep_alive == pkttype:
                            cap = int.from_bytes(ser.read(2), byteorder="big", signed=False)
                            batAvgVoltage = int.from_bytes(ser.read(2), byteorder="big", signed=False)
                            trickle_count = int.from_bytes(ser.read(1), byteorder="big", signed=False)
                            
                            net.processKeepAliveMessage(str(nodeid), trickle_count, float(batAvgVoltage)/1000)
                            if printVerbosity > 1:
                                net.addPrint("Received keep alive packet from node "+ str(nodeid) +" with cap & voltage: "+ str(cap) +" "+ str(batAvgVoltage) +" trickle count: "+ str(trickle_count))
                            appLogger.info("[Node {0}] Received keep alive message with capacity: {1} and voltage: {2} trickle_count {3}".format(nodeid, cap, batAvgVoltage,trickle_count))
                            dataLogger.info("Node {0} 0x {1} 0x {2} 0x {3}{4} {5}".format(nodeid, '{:02x}'.format(pkttype), '{:x}'.format(pktnum), '{:02x}'.format(cap), '{:02x}'.format(batAvgVoltage),trickle_count))

                        else:
                            net.addPrint("Unknown packettype: "+ str(pkttype))
                            appLogger.warning("[Node {0}] Received unknown packettype: {1}".format(nodeid, hex(pkttype)))
                            dataLogger.info("Node {0} 0x {1} 0x {2}".format(nodeid, '{:02x}'.format(pkttype), '{:x}'.format(pktnum)))
                    else:  #start == START_BUF
                        net.addPrint("Unknown START_BUF: "+ start)
                else:   #start == START_CHAR
                        net.addPrint("Unknown START_CHAR: "+ start)
                    
            currentTime = time.time()
            if currentTime - previousTimeTimeout > timeoutInterval:
                previousTimeTimeout = time.time()
                deletedCounter = 0
                for x in range(len(messageSequenceList)):
                    if currentTime - messageSequenceList[x - deletedCounter].latestTime > timeoutTime:
                        # TODO send data before deleting element
                        del messageSequenceList[x - deletedCounter]
                        deletedCounter += 1
                        if printVerbosity > 1:
                            xd = x + deletedCounter
                            net.addPrint("Deleted seqid %d because of timeout" % xd)
                        appLogger.debug("Node Sequence timed out")

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

            net.addPrint('Serial Port closed! Trying to open port: %s'% ser.port)

            try:
                ser.open()
            except Exception as e:
                net.addPrint("Serial Port open exception:"+ str(e))
                appLogger.debug("Serial Port exception: %s", e)
                time.sleep(5)
                continue

            net.addPrint("Serial Port open!")
            appLogger.debug("Serial Port open")


except UnicodeDecodeError as e:
    pass

except KeyboardInterrupt:
    print("-----Packet delivery stats summary-----")
    print("Total packets delivered: ", deliverCounter)
    print("Total packets dropped: ", dropCounter)
    print("Total header packets dropped: ", headerDropCounter)
    print("Packet delivery rate: ", 100 * (deliverCounter / (deliverCounter + dropCounter)))
    print("Messages defragmented: ", defragmentationCounter)
    print("Logs are in: "+logfolderpath)

    appLogger.info("-----Packet delivery stats summary-----")
    appLogger.info("Total packets delivered: {0}".format(deliverCounter))
    appLogger.info("Total packets dropped: {0}".format(dropCounter))
    appLogger.info("Total header packets dropped: {0}".format(headerDropCounter))
    appLogger.info("Packet delivery rate: {0}".format(100 * (deliverCounter / (deliverCounter + dropCounter))))
    appLogger.info("Messages defragmented: {0}".format(defragmentationCounter))
    raise

finally:
    print("done")

