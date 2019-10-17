import serial
import time
import datetime
import logging
from recordtype import recordtype
from enum import IntEnum, unique
from datetime import timedelta
import readchar
import os
import sys
import threading
from collections import deque
import shutil
import binascii
import struct
import math
import pexpect
import json
import traceback
import paho.mqtt.client as mqtt
from itertools import chain, starmap

START_CHAR = '02'
#START_BUF = '2a2a2a'  # this is not really a buffer
BAUD_RATE = 57600 #921600 #1000000
SERIAL_PORT = "/dev/ttyS0" #"/dev/ttyACM0" #"/dev/ttyS0"

if len(sys.argv)>1:
	SERIAL_PORT=sys.argv[1]

if len(sys.argv)>2:
	BAUD_RATE=int(sys.argv[2])


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
MAX_REPORTS_PER_PACKET = 13 #must be the same as in common/inc/contraints.h
MAX_PACKET_PAYLOAD = SINGLE_NODE_REPORT_SIZE*MAX_REPORTS_PER_PACKET

MAX_BEACONS = 100

MAX_ONGOING_SEQUENCES = 10

MAX_TRICKLE_C_VALUE = 256

ser = serial.Serial()
ser.port = SERIAL_PORT
ser.baudrate = BAUD_RATE
ser.timeout = 100
TANSMIT_ONE_CHAR_AT_THE_TIME=True   #Setting this to true makes the communication from the gateway to the sink more reliable. Tested with an ad-hoc firmware. When this is removed the sink receives corrupted data much more frequently
#ser.inter_byte_timeout = 0.1 #not the space between stop/start condition

MessageSequence = recordtype("MessageSequence","timestamp,nodeid,lastPktnum,sequenceSize,datacounter,datalist,latestTime")
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



def initialize_log():
    global filenameConsoleLog
    global fullpathConsoleLog
    global consoleLogger
    global consolelog_handler

    global filenameUARTLog
    global fullpathUARTLog
    global UARTLogger
    global uartlog_handler

    global filenameContactLog
    global fullpathContactLog
    global contactLogger
    global contactlog_handler

    global filenameErrorLog
    global fullpathErrorLog
    global errorLogger
    global errorlog_handler


    formatter = logging.Formatter('%(asctime)s %(levelname)s %(message)s', datefmt='%Y/%m/%d %H:%M:%S')
    timestr = time.strftime("%Y%m%d-%H%M%S")

    nameConsoleLog = "consoleLogger"
    filenameConsoleLog = timestr + "-console.log"
    fullpathConsoleLog = logfolderpath+filenameConsoleLog
    consolelog_handler = logging.FileHandler(fullpathConsoleLog)
    consolelog_handler.setFormatter(formatter)
    consoleLogger = logging.getLogger(nameConsoleLog)
    consoleLogger.setLevel(LOG_LEVEL)
    consoleLogger.addHandler(consolelog_handler)

    nameUARTLog = "UARTLogger"
    filenameUARTLog = timestr + "-UART.log"
    fullpathUARTLog = logfolderpath+filenameUARTLog
    uartlog_handler = logging.FileHandler(fullpathUARTLog)
    uartlog_handler.setFormatter(formatter)
    UARTLogger = logging.getLogger(nameUARTLog)
    UARTLogger.setLevel(LOG_LEVEL)
    UARTLogger.addHandler(uartlog_handler)

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

def close_logs():
    global filenameConsoleLog
    global fullpathConsoleLog
    global consoleLogger
    global consolelog_handler


    global filenameUARTLog
    global fullpathUARTLog
    global UARTLogger
    global uartlog_handler

    global filenameContactLog
    global fullpathContactLog
    global contactLogger
    global contactlog_handler

    global filenameErrorLog
    global fullpathErrorLog
    global errorLogger
    global errorlog_handler

    logging.shutdown()

    consoleLogger.removeHandler(consolelog_handler)
    UARTLogger.removeHandler(uartlog_handler)
    contactLogger.removeHandler(contactlog_handler)
    errorLogger.removeHandler(errorlog_handler)

initialize_log()

firmwareChunkDownloaded_event=threading.Event()
firmwareChunkDownloaded_event_data=[]

MAX_CHUNK_SIZE=256                  #must be the same as OTA_BUFFER_SIZE in ota-download.h and OTA_CHUNK_SIZE in sink_receiver.c
CHUNK_DOWNLOAD_TIMEOUT=7
MAX_SUBCHUNK_SIZE=128               #MAX_SUBCHUNK_SIZE should be always strictly less than MAX_CHUNK_SIZE (if a chunk doesn't get spitted problems might arise, the case is not managed)
if TANSMIT_ONE_CHAR_AT_THE_TIME:
    SUBCHUNK_INTERVAL=0.1
    CHUNK_INTERVAL_ADD=0.1
else:
    SUBCHUNK_INTERVAL=0.02 #with 0.1 it is stable, less who knows? 0.03 -> no error in 3 ota downloads. 0.02 no apparten problem...
    CHUNK_INTERVAL_ADD=0.01

REBOOT_INTERVAL=0.5

octave_files_folder="../data_plotting/matlab_version" #./
contact_log_file_folder=logfolderpath #./
#contact_log_filename="vela-09082019/20190809-141430-contact_cut.log" #filenameContactLog #"vela-09082019/20190809-141430-contact_cut.log"
octave_launch_command="/usr/bin/flatpak run org.octave.Octave -qf" #octave 5 is required. Some distro install octave 4 as default, to overcome this install octave through flatpak
detector_octave_script="run_detector.m"
events_file_json="json_events.txt"

PROCESS_INTERVAL=10
ENABLE_PROCESS_OUTPUT_ON_CONSOLE=False

MQTT_BROKER="iot.smartcommunitylab.it"
MQTT_PORT=1883
DEVICE_TOKEN="iOn25YbEvRBN9sp6xdCd"
TELEMETRY_TOPIC="v1/gateway/telemetry"
CONNECT_TOPIC="v1/gateway/connect"

def flatten_json(dictionary):
    """Flatten a nested json file"""

    def unpack(parent_key, parent_value):
        """Unpack one level of nesting in json file"""
        # Unpack one level only!!!
        
        if isinstance(parent_value, dict):
            for key, value in parent_value.items():
                temp1 = parent_key + '_' + key
                yield temp1, value
        elif isinstance(parent_value, list):
            i = 0 
            for value in parent_value:
                temp2 = parent_key + '_'+str(i) 
                i += 1
                yield temp2, value
        else:
            yield parent_key, parent_value    

            
    # Keep iterating until the termination condition is satisfied
    while True:
        # Keep unpacking the json file until all values are atomic elements (not dictionary or list)
        dictionary = dict(chain.from_iterable(starmap(unpack, dictionary.items())))
        # Terminate condition: not any value in the json file is dictionary or list
        if not any(isinstance(value, dict) for value in dictionary.values()) and \
           not any(isinstance(value, list) for value in dictionary.values()):
            break

    return dictionary

#def on_publish_cb(client, data, result):
#    net.addPrint("[MQTT] on_publish_cb. data: "+str(data)+", result: "+str(result))

def on_disconnect_cb(client, userdata, result):
    global mqtt_connected
    net.addPrint("[MQTT] on_disconnect. result: "+str(result))

    mqtt_connected=False

def on_connect_cb(client, userdata, flags, rc):
    global mqtt_connected
    net.addPrint("[MQTT] on_connect. result code: "+str(rc))

    if rc==0:
        mqtt_connected=True
    else:
        mqtt_connected=False

    #client.subscribe() #in case data needs to be received, subscribe here


def on_message_cb(client, userdata, msg):
    net.addPrint("[MQTT] on_publish_cb. data: "+str(data)+", result: "+str(result))

def connect_mqtt():
    net.addPrint("[MQTT] Connecting to mqtt broker...")

    client=mqtt.Client()
    #client.on_publish=on_publish_cb
    client.on_connect=on_connect_cb
    client.on_disconnect=on_disconnect_cb
    client.on_message=on_message_cb

    client.username_pw_set(DEVICE_TOKEN, password=None)
    client.connect(MQTT_BROKER,MQTT_PORT)

    return client

def publish_mqtt(mqtt_client, data_to_publish, topic):
    global mqtt_connected

    if mqtt_client==None:
        net.addPrint("[MQTT] Not yet connected, cannot publish "+str(data_to_publish))
    
    qos=1
    try:
        ret=mqtt_client.publish(topic, json.dumps(data_to_publish),qos)
        if ret.rc: #print only in case of error
            net.addPrint("[MQTT] publish return: "+str(ret.rc))

        with open("mqtt_store.txt", "a") as f:
            f.write(datetime.datetime.fromtimestamp(time.time()).strftime('%Y-%m-%d %H:%M:%S')+" "+str(data_to_publish)+"\n")
    except Exception:
        net.addPrint("[MQTT] Exception during publishing!!!!")
        
        

def start_poximity_detection():
    global filenameContactLog
    global proximity_detector_in_queue

    file_to_process=filenameContactLog
    close_logs()
    initialize_log()
    proximity_detector_in_queue.append(file_to_process)

class PROXIMITY_DETECTOR_POLL_THREAD(threading.Thread):
    def __init__(self, threadID, name):
        threading.Thread.__init__(self)
        self.threadID= threadID
        self.name = name
        self.__loop=True


    def run(self):
        global mqtt_connected
        global mqtt_client
        global dummy_c

        mqtt_connected=False
        mqtt_client=connect_mqtt()
        dummy_c=0
        mqtt_client.loop_start()

        while self.__loop:
            time.sleep(PROCESS_INTERVAL)
            start_poximity_detection()
            

class PROXIMITY_DETECTOR_THREAD(threading.Thread):
    def __init__(self, threadID, name):
        threading.Thread.__init__(self)
        self.threadID= threadID
        self.name = name
        self.__loop=True
        self.file_processed=True

    def get_result(self):
        try:
            with open(octave_files_folder+'/'+events_file_json) as f: # Use file to refer to the file object
                data=f.read()
                return json.loads(data)
        except OSError:
            return None
            
        return None

    def on_processed(self):
        net.addPrint("[EVENT EXTRACTOR] Contact log file processed!")
        self.file_processed=True
        #octave_process.kill(0)
        jdata=self.get_result()
        try:
            evts=jdata["proximity_events"]

            if evts!=None:
                self.on_events(evts)
                os.remove(octave_files_folder+'/'+events_file_json) #remove the file to avoid double processing of it

                evts=None
            else:
                net.addPrint("[EVENT EXTRACTOR] No event found!")
        except KeyError:
            net.addPrint("[EVENT EXTRACTOR] No event found!")
            return

    def on_events(self,evts):
        global mqtt_client
        global dummy_c

        #with open("proximity_event_store.txt", "a") as f:
            #json.dump(evts,f)
        #    f.write(datetime.datetime.fromtimestamp(time.time()).strftime('%Y-%m-%d %H:%M:%S')+" "+str(evts)+"\n")
        net.addPrint("[EVENT EXTRACTOR] Events: "+str(evts[0:3])) #NOTE: At this point the variable evts contains the proximity events. As example I plot the first 10

        net.addPrint("[EVENT EXTRACTOR] Obtaining network status") #NOTE: At this point the variable evts contains the proximity events. As example I plot the first 10
        net_descr=net.obtainNetworkDescriptorObject()
        #with open("network_status_store.txt", "a") as f:
            #json.dump(net_descr,f)
        #    f.write(datetime.datetime.fromtimestamp(time.time()).strftime('%Y-%m-%d %H:%M:%S')+" "+str(net_descr)+"\n")
        net.addPrint("[EVENT EXTRACTOR] Network status: "+str(net_descr))

        #dummy_c=dummy_c+1
        #dummy_data={"Test":dummy_c}
        #publish_mqtt(mqtt_client, dummy_data, TELEMETRY_TOPIC)

        t_string=datetime.datetime.fromtimestamp(time.time()).strftime('%Y-%m-%d %H:%M:%S')

        with open("proximity_event_store.txt", "a") as f:
            for ev in evts:
                ev_flat=flatten_json(ev)    # ev should be already single level. To avoid problems flatten it!
                #ev_flat["ts"]=int(time.time()*1000)
                #publish_mqtt(mqtt_client, ev_flat, TELEMETRY_TOPIC) //In order to publish with the device gateway defined in thingsboard, I need to have the correct node lable, otherwise the data won't be assigned to the correct node
                f.write(t_string+" "+str(ev_flat)+"\n")

        #with open("network_status_store.txt", "a") as f:
        publish_mqtt(mqtt_client, net_descr, TELEMETRY_TOPIC)
            #f.write(t_string+" "+str(net_descr)+"\n")

            #for n_descr in net_descr:
            #    n_descr_flat=flatten_json(n_descr)
            #    n_descr_flat["ts"]=int(time.time()*1000)
            #    publish_mqtt(mqtt_client, n_descr_flat, TELEMETRY_TOPIC)
            #    f.write(t_string+" "+str(n_descr_flat)+"\n")
            
            

    def run(self):
        
        octave_process=None
        
        octave_to_log_folder_r_path=os.path.relpath(contact_log_file_folder, octave_files_folder)

        while self.__loop:
            try:
                if self.file_processed:
                    file_to_process=proximity_detector_in_queue.popleft()
                    self.file_processed=False

                    command=octave_launch_command+" "+detector_octave_script+" "+octave_to_log_folder_r_path+'/'+file_to_process
                    net.addPrint("[EVENT EXTRACTOR] Sending this command: "+command)
                    octave_process=pexpect.spawnu(command,echo=False,cwd=octave_files_folder)

                if ENABLE_PROCESS_OUTPUT_ON_CONSOLE:
                    octave_process.expect("\n",timeout=60)
                    octave_return=octave_process.before.split('\r')
                    for s in octave_return:
                        if len(s)>0:
                            net.addPrint("[EVENT EXTRACTOR] OCTAVE>>"+s)
                else:
                    octave_process.expect(pexpect.EOF,timeout=5)
                    self.on_processed()

            except pexpect.exceptions.TIMEOUT:
                net.addPrint("[EVENT EXTRACTOR] Octave is processing...")
                pass
            except pexpect.exceptions.EOF:
                self.on_processed()
                pass
            except IndexError:
                time.sleep(1)
                pass




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
        self.__uartLogLines=0
        self.__trickleQueue = deque()
        self.showHelp=False

    def getNode(self, label):
        for n in self.__nodes:
            if n.name == label:
                return n

        return None

    def addNode(self,n):
        global mqtt_client

        self.__nodes.append(n)
        if len(self.__nodes) == 1:
            self.__expTrickle=n.lastTrickleCount

        #notify thingsboard that a new node has join the network
        connect_data={"device":n.name}
        publish_mqtt(mqtt_client, connect_data, CONNECT_TOPIC)

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

    def rebootNetwork(self,reboot_delay_ms):
        self.addPrint("[APPLICATION] Rebooting all the nodes...")
        for n in self.__nodes:
            destination_ipv6_addr=int(n.name,16)
            self.rebootNode(destination_ipv6_addr,reboot_delay_ms)
            time.sleep(REBOOT_INTERVAL)

    def rebootNode(self,destination_ipv6_addr,reboot_delay_ms):
        #destination_ipv6_addr=0xFD0000000000000002124B000F24DD87
        
        #reboot_delay_ms=10000
        breboot_delay_ms = reboot_delay_ms.to_bytes(4, byteorder="big", signed=False)
        payload = breboot_delay_ms

        if destination_ipv6_addr!=None:
            self.sendNewRipple(destination_ipv6_addr,build_outgoing_serial_message(PacketType.ota_reboot_node, payload))
        else:
            self.sendNewTrickle(build_outgoing_serial_message(PacketType.ota_reboot_node, payload),True)    #by default ingore any ongoing trickle queue and force the reboot
            

    def startFirmwareUpdate(self,autoreboot=True):
        REBOOT_DELAY_S=20
        nodes_updated=[]
        MAX_OTA_ATTEMPTS=100

        node_firmware=None
        #firmware_filename="./../../../network_board/basic_sender_test/vela_node_ota"
        firmware_filename="./../../../network_board/integrate_sender_uart/vela_node_ota"
        try:        
            with open(firmware_filename, "rb") as f:
                node_firmware = f.read()

            if node_firmware!=None:
                self.addPrint("[APPLICATION] Firmware binary: "+firmware_filename+" loaded. Size: "+str(len(node_firmware)))
                METADATA_LENGTH=14
                metadata_raw=node_firmware[0:METADATA_LENGTH]

                fw_metadata_crc_int = int.from_bytes(metadata_raw[0:2], byteorder="little", signed=False)
                fw_metadata_crc_shadow_int = int.from_bytes(metadata_raw[2:4], byteorder="little", signed=False)
                fw_metadata_size_int = int.from_bytes(metadata_raw[4:8], byteorder="little", signed=False)
                fw_metadata_uuid_int = int.from_bytes(metadata_raw[8:12], byteorder="little", signed=False)
                fw_metadata_version_int = int.from_bytes(metadata_raw[12:14], byteorder="little", signed=False)

                fw_metadata_crc_str = "{0}".format(hex(fw_metadata_crc_int))
                fw_metadata_crc_shadow_str = "{0}".format(hex(fw_metadata_crc_shadow_int))
                fw_metadata_size_str = "{0}".format(hex(fw_metadata_size_int))
                fw_metadata_uuid_str = "{0}".format(hex(fw_metadata_uuid_int))
                fw_metadata_version_str = "{0}".format(hex(fw_metadata_version_int))

                self.addPrint("fw_metadata_crc_str: "+str(fw_metadata_crc_str))
                self.addPrint("fw_metadata_crc_shadow_str: "+str(fw_metadata_crc_shadow_str))
                self.addPrint("fw_metadata_size_str: "+str(fw_metadata_size_str))
                self.addPrint("fw_metadata_uuid_str: "+str(fw_metadata_uuid_str))
                self.addPrint("fw_metadata_version_str: "+str(fw_metadata_version_str))

                fw_metadata=dict()
                fw_metadata["crc"]=fw_metadata_crc_int
                fw_metadata["crc_shadow"]=fw_metadata_crc_shadow_int
                fw_metadata["size"]=fw_metadata_size_int
                fw_metadata["uuid"]=fw_metadata_uuid_int
                fw_metadata["version"]=fw_metadata_version_int

            else:
                self.addPrint("[APPLICATION] Error during firmware file read, aborting ota")
                return

        except OSError:
            self.addPrint("[APPLICATION] Exception during firmware file read (check if the file "+firmware_filename+" is there)")
            return

        for n in self.__nodes:
            try:
                if fw_metadata["version"] > n.fw_metadata["version"]:
                    attempts=0
                    destination_ipv6_addr=int(n.name,16)
                    self.addPrint("[APPLICATION] Starting firmware update on node: 0x " +str(n.name))

                    #self.sendFirmware(destination_ipv6_addr,node_firmware)
                    while(self.sendFirmware(destination_ipv6_addr,node_firmware) != 0 and attempts<MAX_OTA_ATTEMPTS):
                        self.addPrint("[APPLICATION] Retrying firmware update on node: 0x " +str(n.name))
                        attempts+=1

                    if attempts<MAX_OTA_ATTEMPTS:
                        nodes_updated.append(n)
                else:
                    self.addPrint("[APPLICATION] Node: 0x "+str(n.name)+" is already up to date.")
            except KeyError:
                self.addPrint("[APPLICATION] Exception during the ota for Node: 0x "+str(n.name))
                
        if autoreboot:
            self.addPrint("[APPLICATION] Automatically rebooting the nodes that correctly ended the OTA process in: "+str(REBOOT_DELAY)+"s")
            time.sleep(5) #just wait a bit
            for n in nodes_updated:
                destination_ipv6_addr=int(n.name,16)
                self.rebootNode(destination_ipv6_addr,REBOOT_DELAY_S*1000) #Once the reboot command is received by the node, it will start a timer that reboot the node in REBOOT_DELAY
                time.sleep(REBOOT_INTERVAL)
            

    def sendFirmware(self, destination_ipv6_addr, firmware):
        global NO_OF_CHUNKS_IN_THE_FIRMWARE
        MAX_OTA_TIMEOUT_COUNT=100
        EXPECT_ACK_FOR_THE_LAST_PACKET=True #in some cases the CRC check performed with the arrival of the last packet causes the node to crash because of stack overflow. In that case, the last ack won't be received, and if we don't manage this the GW consider the OTA process aborted because of timeout and repeat it.
        #Then when the memory overflow happens we can set the flag EXPECT_ACK_FOR_THE_LAST_PACKET to false so that the OTA process goes on for the rest of the network.


        firmwaresize=len(firmware)
        NO_OF_CHUNKS_IN_THE_FIRMWARE=math.ceil(firmwaresize/MAX_CHUNK_SIZE)

        offset=0
        chunk_no=1
        chunk_timeout_count=0
        lastCorrectChunkReceived=1 #might it be 0?

        while offset<firmwaresize and chunk_timeout_count<MAX_OTA_TIMEOUT_COUNT:
            if offset+MAX_CHUNK_SIZE<firmwaresize:
                chunk=firmware[offset:offset+MAX_CHUNK_SIZE]
            else:
                chunk=firmware[offset:]

            self.addPrint("[DEBUG] sending firmware chunk number: "+str(chunk_no) + "/"+str(NO_OF_CHUNKS_IN_THE_FIRMWARE)+" - "+str((int((offset+MAX_CHUNK_SIZE)*10000/firmwaresize))/100)+"%")
            
            chunksize=len(chunk)
            self.sendFirmwareChunk(destination_ipv6_addr,chunk,chunk_no)
            
            self.addPrint("[DEBUG] Chunk sent, waiting ack...")
            
            #wait the response from the remote node
            if chunk_no==1:
                time_to_wait=10+CHUNK_DOWNLOAD_TIMEOUT #the first packet needs more time because the OTA module is initialized
            else:
                time_to_wait=CHUNK_DOWNLOAD_TIMEOUT
                            
            if firmwareChunkDownloaded_event.wait(time_to_wait):
                firmwareChunkDownloaded_event.clear()
                time.sleep(CHUNK_INTERVAL_ADD)

                if chunk_no != NO_OF_CHUNKS_IN_THE_FIRMWARE: #not the last chunk
                    zero=0
                    chunk_no_b=chunk_no%256
                    ackok=chunk_no_b.to_bytes(1, byteorder="big", signed=False)+zero.to_bytes(1, byteorder="big", signed=True)
                    if firmwareChunkDownloaded_event_data==ackok:
                        chunk_timeout_count=0
                        self.addPrint("[DEBUG] OTA ack OK!!") #ok go on
                        lastCorrectChunkReceived=chunk_no
                    else:                   
                        self.addPrint("[DEBUG] OTA ack NOT OK!! Err: "+str(firmwareChunkDownloaded_event_data[1])+". The chunk will be retrasmitted in a moment...")
                        time.sleep(5) #in case a full chunk is transmitted to the node twice (because of a ota_ack loss), we might get multiple nacks for a chunk
                        while firmwareChunkDownloaded_event.isSet():
                            self.addPrint("[DEBUG] More than one ACK/NACK received for this firmware chunk. I should be able to recover...just give me some time")
                            firmwareChunkDownloaded_event.clear()
                            lastCorrectChunkReceived=firmwareChunkDownloaded_event_data[0]+256*int(chunk_no/256)
                            time.sleep(5)

                    chunk_no=lastCorrectChunkReceived + 1
                    #self.addPrint("[DEBUG] I'll send chunk_no: "+str(chunk_no)+" in a moment")
                    offset=(chunk_no-1)*MAX_CHUNK_SIZE
                else:
                    zero=0
                    one=1
                    chunk_no_b=chunk_no%256
                    ackok=chunk_no_b.to_bytes(1, byteorder="big", signed=False)+zero.to_bytes(1, byteorder="big", signed=True) #everithing fine on the node
                    ackok_alt=chunk_no_b.to_bytes(1, byteorder="big", signed=False)+one.to_bytes(1, byteorder="big", signed=True) #crc ok, but there was memory overflow during CRC verification. The firmware will be vefiried on the next reboot
                    if firmwareChunkDownloaded_event_data==ackok:
                        chunk_no+=1
                        offset=(chunk_no-1)*MAX_CHUNK_SIZE
                        return 0 #OTA update correctly closed
                    elif firmwareChunkDownloaded_event_data==ackok_alt:
                        self.addPrint("[OTA] CRC ok but not written on the node...")
                        chunk_no+=1
                        offset=(chunk_no-1)*MAX_CHUNK_SIZE
                        return 0 #OTA update correctly closed even if a memory overflow happened on the node during CRC verification. The firmware will be vefiried on the next reboot
                    else:
                        self.addPrint("[OTA] CRC error at the node...")
                        return 1 #crc error at the node
            else:
                if chunk_no == NO_OF_CHUNKS_IN_THE_FIRMWARE and not EXPECT_ACK_FOR_THE_LAST_PACKET: #the last packet
                    return 0 #OTA update correctly closed
                chunk_timeout_count+=1
                self.addPrint("[DEBUG] Chunk number "+str(chunk_no)+" ack timeout, retry!")


    def sendFirmwareChunk(self, destination_ipv6_addr, chunk,chunk_no):
        global NO_OF_CHUNKS_IN_THE_FIRMWARE
        global NO_OF_SUB_CHUNKS_IN_THIS_CHUNK

        chunksize=len(chunk)
        NO_OF_SUB_CHUNKS_IN_THIS_CHUNK=math.ceil(chunksize/MAX_SUBCHUNK_SIZE)
        
        sub_chunk_no=0
        offset=0
        while offset<chunksize:
            sub_chunk_no+=1
            if offset+MAX_SUBCHUNK_SIZE < chunksize:
                sub_chunk=chunk[offset:offset+MAX_SUBCHUNK_SIZE]
                offset=offset+MAX_SUBCHUNK_SIZE
            else:
                sub_chunk=chunk[offset:]
                offset=chunksize
            
            #self.addPrint("[DEBUG] sending firmware subchunk number: "+str(sub_chunk_no) + "/"+str(NO_OF_SUB_CHUNKS_IN_THIS_CHUNK))           

            self.sendFirmwareSubChunk(destination_ipv6_addr, sub_chunk,chunk_no,sub_chunk_no)
            if sub_chunk_no != NO_OF_SUB_CHUNKS_IN_THIS_CHUNK:
                time.sleep(SUBCHUNK_INTERVAL)

    def sendFirmwareSubChunk(self, destination_ipv6_addr, sub_chunk, chunk_no, sub_chunk_no):
        global NO_OF_CHUNKS_IN_THE_FIRMWARE
        global NO_OF_SUB_CHUNKS_IN_THIS_CHUNK
        
        if chunk_no==1 and sub_chunk_no==1:
            packetType=PacketType.ota_start_of_firmware   #start of firmware
            self.addPrint("[DEBUG] PacketType start of firmware")
            self.addPrint("[DEBUG] Subchunk size: " +str(len(sub_chunk)))
           
        elif chunk_no==NO_OF_CHUNKS_IN_THE_FIRMWARE and sub_chunk_no==NO_OF_SUB_CHUNKS_IN_THIS_CHUNK:
            packetType=PacketType.ota_end_of_firmware   #end of firmware
            self.addPrint("[DEBUG] PacketType end of firmware")
        else:
            packetType=PacketType.ota_more_of_firmware   #all other packets

        chunk_no_8b=chunk_no%256
        sub_chunk_no_8b=sub_chunk_no%256

        data=chunk_no_8b.to_bytes(1, byteorder="big", signed=False)+sub_chunk_no_8b.to_bytes(1, byteorder="big", signed=False)+sub_chunk
        
        self.sendNewRipple(destination_ipv6_addr,build_outgoing_serial_message(packetType, data))


    def sendNewRipple(self, destination_addr, message):
        destination_addr_bytes=destination_addr.to_bytes(length=16, byteorder='big', signed=False)

        destination_addr_ascii=''.join('%02X'%i for i in destination_addr_bytes)
        destination_addr_ascii_encoded=destination_addr_ascii.encode('utf-8')
        message=destination_addr_ascii_encoded+message

        #net.addPrint("[APPLICATION] Ripple message going to be sent...")
        send_serial_msg(message)
        #net.addPrint("[APPLICATION] Ripple message sent.")


    def sendNewTrickle(self, message,forced=False):
        broadcast_ipv6_addr=0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF
        destination_addr_bytes=broadcast_ipv6_addr.to_bytes(length=16, byteorder='big', signed=False)

        destination_addr_ascii=''.join('%02X'%i for i in destination_addr_bytes)
        destination_addr_ascii_encoded=destination_addr_ascii.encode('utf-8')
        message=destination_addr_ascii_encoded+message

        if forced or destination_addr_bytes==broadcast_ipv6_addr:
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

    def obtainNetworkDescriptorObject(self):
        net_descr=dict()
        for n in self.__nodes:
            n_descr=n.getNodeDescriptorObject()
            n_descr_flat=flatten_json(n_descr)
            net_descr[n.name]=[n_descr_flat]

        return net_descr

    def printNetworkStatus(self):

        if(float(time.time()) - self.__lastNetworkPrint < 0.2): #this is to avoid too fast call to this function
            return

        __lastNetworkPrint = float(time.time())

        netSize = len(self.__nodes)

        if CLEAR_CONSOLE:
            cls()

        print("|-----------------------------------------------------------------------------------------------------------------------------------------|")
        print("|-------------------------------------------------|  Network size %3s log lines %7s |-------------------------------------------------|" %(str(netSize), self.__uartLogLines))
        print("|----------------------------------------| Trickle: min %3d; max %3d; exp %3d; queue size %2d |--------------------------------------------|" %(self.__netMinTrickle, self.__netMaxTrickle, self.__expTrickle, len(self.__trickleQueue)))
        print("|-----------------------------------------------------------------------------------------------------------------------------------------|")
        print("| NodeID | Battery                              | Last    | Firmware | Trick |   #BT  |                             Neighbors info string |")
        print("|        | Volt   SoC Capacty    Cons    Temp   | seen[s] | version  | Count |   Rep  |     P: a parent,  P*: actual parent,  N: neighbor |")
        #print("|           |                 |                     |             |            |")
        for n in self.__nodes:
            n.printNodeInfo()
        #print("|           |                 |                     |             |            |")
        print("|-----------------------------------------------------------------------------------------------------------------------------------------|")
        if self.showHelp:
            print("|    AVAILABLE COMMANDS:")
            print("| key    command\n|")
            print("| 1      request ping\n"
                  "| 2      enable bluetooth\n"
                  "| 3      disable bluetooth\n"
                  "| 4      bt_def\n"
                  "| 5      bt_with_params\n"
                  "| 8      reset nordic\n"
                  "| 9      set time between sends\n"
                  "| >9     set keep alive interval in seconds")
        else:
            print("|     h+enter    : Show available commands                                                                                                |")
        print("|-----------------------------------------------------------------------------------------------------------------------------------------|")
        print("|-----------------------------------------------|            CONSOLE                     |------------------------------------------------|")
        print("|-----------------------------------------------------------------------------------------------------------------------------------------|")

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

    def processKeepAliveMessage(self, label, trickleCount, battery_data, fw_metadata, nbr_string=None):
        n = self.getNode(label)
        if n != None:
            n.online=True 
        else:
            n=Node(label, trickleCount)
            self.addNode(n)
        
        n.updateTrickleCount(trickleCount)
        n.updateBatteryData(battery_data)
        
        n.updateFWMetadata(fw_metadata)
        
        if nbr_string==None:
            nbr_string=""
        
        n.updateNbrString(nbr_string)
        
        if len(self.__trickleQueue) != 0:
            if self.__trickleCheck():
                self.__expTrickle = (self.__netMaxTrickle + 1)%MAX_TRICKLE_C_VALUE
                message=self.__trickleQueue.popleft()
                send_serial_msg(message)
                self.addPrint("[APPLICATION] Trickle message: 0x {0} automatically sent".format((message).hex()))
        else:
            self.__trickleCheck()
        self.printNetworkStatus()

    def processBTReportMessage(self, label):
        n = self.getNode(label)
        if n != None:
            n.BTReportHandler()
        #else:
        #    n=Node(label, 0)
        #    self.addNode(n)
        #    n.BTReportHandler()

    def processUARTError(self):
        self.__uartLogLines=self.__uartLogLines+1

    def resetNodeTimeout(self, label):
        n = self.getNode(label)
        if n != None:
            n.resetNodeTimeout()

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
    def __init__(self, label, trickleCount=0):
        self.lock = threading.Lock()
        self.name = label
        self.lastTrickleCount = trickleCount
        self.lastMessageTime = float(time.time())
        self.batteryData=None
        self.amountOfBTReports = 0
        self.online=True
        self.fw_metadata=None
        self.nbr_string=""

    def updateTrickleCount(self,trickleCount):
        with self.lock:
            self.lastTrickleCount = trickleCount
            self.lastMessageTime = float(time.time())

    def updateBatteryData(self,batteryData):
        with self.lock:
            self.batteryData=batteryData

    def BTReportHandler(self):
        with self.lock:
            self.amountOfBTReports = self.amountOfBTReports + 1
            self.lastMessageTime = float(time.time())

    def updateFWMetadata(self, fw_metadata):
        with self.lock:
            self.fw_metadata=fw_metadata

    def getMetadataVersionString(self):
        if(self.fw_metadata!=None):
            fw_metadata_version_str = "{0}".format(hex(self.fw_metadata["version"]))
            return fw_metadata_version_str
        else:
            return ""

    def updateNbrString(self,nbr_string):
        with self.lock:
            self.nbr_string=nbr_string

    def getLastMessageElapsedTime(self):
        now = float(time.time())
        return now-self.lastMessageTime

    def resetNodeTimeout(self):
        with self.lock:
            self.lastMessageTime = float(time.time())
 
    def getNodeDescriptorObject(self):
        desc=dict()
        with self.lock:
            desc["name"]=self.name
            #desc["ts"]=int(time.time()*1000)
            desc["battery"]=self.batteryData
            desc["firmware"]=self.fw_metadata
            desc["neighbors_info"]=self.nbr_string
            desc["online"]=self.online
            desc["number_of_bt_reports"]=self.amountOfBTReports
            
        return desc

    def printNodeInfo(self):
        if self.online:
            onlineMarker=' '
        else:
            onlineMarker='*'

        if self.batteryData != None and self.fw_metadata != None:
            print("| %3s%1s| %3.2fV %3.0f%% %4.0fmAh %6.1fmA  %5.1f°  |  %6.0f |   %5s  |   %3d | %6d | %49s |" % (str(self.name[-6:]), onlineMarker, self.batteryData["voltage"], self.batteryData["state_of_charge"], self.batteryData["capacity"], self.batteryData["consumption"], self.batteryData["temperature"], self.getLastMessageElapsedTime(), self.getMetadataVersionString(), self.lastTrickleCount, self.amountOfBTReports, self.nbr_string))
        else:
            print("| %3s |                                       |  %6.0f |          |   %3d | %6d |                                                 |" % (str(self.name[-6:]), self.getLastMessageElapsedTime(), self.lastTrickleCount, self.amountOfBTReports))
        
@unique
class PacketType(IntEnum):
    ota_start_of_firmware =         0x0020  # OTA start of firmware (sink to node)
    ota_more_of_firmware =          0x0021  # OTA more of firmware (sink to node)
    ota_end_of_firmware =           0x0022  # OTA end of firmware (sink to node)
    ota_ack =                       0x0023  # OTA firmware packet ack (node to sink)
    ota_reboot_node =               0x0024  # OTA reboot (node to sink)
    network_new_sequence =          0x0100
    network_active_sequence =       0x0101
    network_last_sequence =         0x0102
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

def cls():
    os.system('cls' if os.name=='nt' else 'clear')

class USER_INPUT_THREAD(threading.Thread):
    def __init__(self, threadID, name):
        threading.Thread.__init__(self)
        self.threadID= threadID
        self.name = name
        self.__loop=True

    def run(self):
        global proximity_detector_in_queue
        while self.__loop:
            try:
                ble_tof_enabled=False
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
                    elif input_str=='f':
                        net.addPrint("[USER_INPUT] Firmware update start.")
                        net.startFirmwareUpdate()
                        continue
                    elif input_str=='b':
                        net.addPrint("[USER_INPUT] Network reboot requested!")
                        net.rebootNetwork(5000)
                        continue
                    elif input_str=='p':
                        net.addPrint("[USER_INPUT] Process contacts log!")
                        start_poximity_detection()
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
                    net.addPrint("[USER_INPUT] Deprecated command, ignored!")
                    #bat_info_interval_s = 90
                    #net.addPrint("[USER_INPUT] Enable battery info with interval: "+str(bat_info_interval_s))
                    #net.sendNewTrickle(build_outgoing_serial_message(PacketType.ti_set_batt_info_int, bat_info_interval_s.to_bytes(1, byteorder="big", signed=False)),forced)
                elif user_input == 7:
                    net.addPrint("[USER_INPUT] Deprecated command, ignored!")
                    #bat_info_interval_s = 0
                    #net.addPrint("[USER_INPUT] Disable battery info")
                    #net.sendNewTrickle(build_outgoing_serial_message(PacketType.ti_set_batt_info_int, bat_info_interval_s.to_bytes(1, byteorder="big", signed=False)),forced)
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
            except Exception as e:
                net.addPrint("[USER_INPUT] Read failed. Read data: "+ input_str)
                traceback.print_exc()
                pass

def build_outgoing_serial_message(pkttype, ser_payload):
    payload_size = 0

    if ser_payload is not None:
        payload_size = len(ser_payload)

    #the packet data starts with the size of the payload
    packet = payload_size.to_bytes(length=1, byteorder='big', signed=False) + pkttype.to_bytes(length=2, byteorder='big', signed=False)

    #add the payload
    if ser_payload is not None:
        packet=packet+ser_payload

    #calculate the payload checksum
    cs=0
    if ser_payload!=None:
        for c in ser_payload:                                       #calculate the checksum on the payload and the len field
            cs+=int.from_bytes([c], "big", signed=False) 
    cs=cs%256                                                   #trunkate to 8 bit
    
    #net.addPrint("[DEBUG] Packet checksum: "+str(cs))
    
    #add the checksum
    packet=packet+cs.to_bytes(1, byteorder='big', signed=False)

    #net.addPrint("[DEBUG] Packet : "+str(packet))
    ascii_packet=''.join('%02X'%i for i in packet)

    return ascii_packet.encode('utf-8')

def send_serial_msg(message):

    line = message + SER_END_CHAR
    
    send_char_by_char=TANSMIT_ONE_CHAR_AT_THE_TIME

    if send_char_by_char:
        for c in line: #this makes it deadly slowly (1 byte every 2ms more or less). However during OTA this makes the transfer safer if small buffer on the sink is available
            ser.write(bytes([c]))
    else:
        ser.write(line)

    ser.flush()
    UARTLogger.info('[GATEWAY] ' + str(line))

def decode_payload(payload, seqid, size, packettype, pktnum): #TODO: packettype can be removed
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


net = Network()
net.addPrint("[APPLICATION] Starting...")
mqtt_client=None

user_input_thread=USER_INPUT_THREAD(4,"user input thread")
user_input_thread.setDaemon(True)
user_input_thread.start()

proximity_detector_in_queue=deque()
proximity_detector_thread=PROXIMITY_DETECTOR_THREAD(5,"proximity detection process")
proximity_detector_thread.setDaemon(True)
proximity_detector_thread.start()


proximity_detec_poll_thread=PROXIMITY_DETECTOR_POLL_THREAD(6,"periodic proximity detection trigger process")
proximity_detec_poll_thread.setDaemon(True)
proximity_detec_poll_thread.start()


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
                        source_addr_bytes=line[cursor:cursor+16]
                        nodeid_int = int.from_bytes(source_addr_bytes, "big", signed=False) 
                        nodeid='%(nodeid_int)X'%{"nodeid_int": nodeid_int}
                        cursor+=16
                        pkttype = int(line[cursor:cursor+2].hex(),16)
                        cursor+=2
                        pktnum = int.from_bytes(line[cursor:cursor+1], "little", signed=False) 
                        cursor+=1

                        if printVerbosity > 0:
                            net.addPrint("[NODEID " + str(nodeid) + "] pkttype " + hex(pkttype) + ", pktnum " + str(pktnum))

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
                            seqid = get_sequence_index(nodeid)

                            #at this point, since we just received 'network_new_sequence', there should not be any sequence associated with this nodeid
                            if seqid != -1: #if this is true something went wrong (duplicate or packet lost from previous report)
                                if messageSequenceList[seqid].lastPktnum == pktnum: #duplicate
                                    if printVerbosity > 1:
                                        net.addPrint("  [PACKET DECODE] Duplicate packet from node "+ str(nodeid)+ " with pktnum "+ str(pktnum))
                                    continue
                                else:                                               #'network_new_sequence' arrived before 'network_last_sequence'. Probably the last packet of the previous report has been lost
                                    if printVerbosity > 1:
                                        net.addPrint("  [PACKET DECODE] Previous sequence has not been completed yet")
                                    # TODO what to do now? For now, assume last packet was dropped
                                    log_contact_data(seqid)                         # store received data instead of deleting it all
                                    del messageSequenceList[seqid]                  # remove data already stored

                            messageSequenceList.append(MessageSequence(datetime.datetime.fromtimestamp(time.time()).strftime('%Y-%m-%d %H:%M:%S'), nodeid, pktnum, 0, 0, [], time.time()))
                            seqid = len(messageSequenceList) - 1
                            messageSequenceList[seqid].sequenceSize += datalen

                            if messageSequenceList[seqid].sequenceSize > MAX_PACKET_PAYLOAD: #this is the typical behaviour
                                decode_payload(payload, seqid, MAX_PACKET_PAYLOAD, PacketType.network_new_sequence, pktnum)
                            else:   #this is when the sequence is made by only one packet.
                                decode_payload(payload, seqid, datalen, PacketType.network_new_sequence, pktnum)

                                if printVerbosity > 1:
                                    net.addPrint("  [PACKET DECODE] Bluetooth sequence decoded. Contact elements: "+ str(len(messageSequenceList[seqid].datalist)))
                                log_contact_data(seqid)
                                net.processBTReportMessage(str(nodeid))

                                del messageSequenceList[seqid]

                        elif PacketType.network_active_sequence == pkttype:
                            seqid = get_sequence_index(nodeid)
                            payload = line[cursor:].hex()
                            cursor = len(line)
                            if seqid == -1:             #if this is true something went wrong. Maybe we have lost the network_new_sequence packet of this report. It means we received 'network_active_sequence' before receiving a valid 'network_new_sequence'
                                if printVerbosity > 1:
                                    net.addPrint("  [PACKET DECODE] First part of sequence dropped, creating incomplete sequence at index "+ str(len(messageSequenceList)) +" from pktnum "+ str(pktnum))
                                messageSequenceList.append( MessageSequence(datetime.datetime.fromtimestamp(time.time()).strftime('%Y-%m-%d %H:%M:%S'), nodeid, pktnum, -1, 0, [], time.time()))
                                seqid = len(messageSequenceList) - 1
                                headerDropCounter += 1

                            elif messageSequenceList[seqid].lastPktnum == pktnum: #duplicate
                                if printVerbosity > 1:
                                    net.addPrint("  [PACKET DECODE] Duplicate packet from node "+ str(nodeid) +" with pktnum "+ str(pktnum))
                                continue

                            messageSequenceList[seqid].lastPktnum = pktnum
                            messageSequenceList[seqid].latestTime = time.time()
                            decode_payload(payload,seqid, MAX_PACKET_PAYLOAD, PacketType.network_active_sequence, pktnum)

                        elif PacketType.network_last_sequence == pkttype:
                            seqid = get_sequence_index(nodeid)
                            payload = line[cursor:].hex()
                            cursor = len(line)
                            if seqid == -1:             #if this is true something went wrong. Maybe we just received network_last_sequence for this report. It means we received 'network_last_sequence' before receiving a valid 'network_new_sequence'
                                messageSequenceList.append(MessageSequence(datetime.datetime.fromtimestamp(time.time()).strftime('%Y-%m-%d %H:%M:%S'), nodeid, pktnum, 0, 0, [], time.time()))
                                seqid = len(messageSequenceList) - 1

                                decode_payload(payload,seqid, MAX_PACKET_PAYLOAD, PacketType.network_last_sequence, pktnum)
                                if printVerbosity > 1:
                                    net.addPrint("  [PACKET DECODE] Bluetooth sequence decoded but header files were never received.  datacounter: "+ str(messageSequenceList[seqid].datacounter) +" ContactData elements: "+ str(len(messageSequenceList[seqid].datalist)))

                                headerDropCounter += 1
                                log_contact_data(seqid)
                                del messageSequenceList[seqid]

                            elif messageSequenceList[seqid].sequenceSize == -1: # if we have lost network_new_sequence, but at least one network_active_sequence is correctly received, sequense size is initialized to -1 (the real value was in network_new_sequence, therefore we lost it)
                                decode_payload(payload,seqid, MAX_PACKET_PAYLOAD, PacketType.network_last_sequence, pktnum)
                                if printVerbosity > 1:
                                    net.addPrint("  [PACKET DECODE] Bluetooth sequence decoded but header files were never received.  datacounter: "+ str(messageSequenceList[seqid].datacounter) +" ContactData elements: "+ str(len(messageSequenceList[seqid].datalist)))
                                log_contact_data(seqid)
                                del messageSequenceList[seqid]

                            else:                       # normal behaviour
                                remainingDataSize = messageSequenceList[seqid].sequenceSize - messageSequenceList[seqid].datacounter
                                #TODO: remainingDataSize now should be equal to the size of this payload, instead of using inequalities use straigt ==. But carefully check the cases and sizes
                                if remainingDataSize > MAX_PACKET_PAYLOAD: #the value in sequenceSize is probably not correct. Did we lost a lot of packets?
                                    decode_payload(payload,seqid, MAX_PACKET_PAYLOAD, PacketType.network_last_sequence, pktnum)
                                else:                   # normal behaviour
                                    decode_payload(payload,seqid, remainingDataSize, PacketType.network_last_sequence, pktnum)

                                if messageSequenceList[seqid].sequenceSize != messageSequenceList[seqid].datacounter:
                                    if printVerbosity > 1:
                                        net.addPrint("  [PACKET DECODE] ERROR: Messagesequence ended, but datacounter is not equal to sequencesize")

                                if printVerbosity > 1:
                                    net.addPrint("  [PACKET DECODE] Bluetooth sequence decoded. "+" sequencesize: "+ str(messageSequenceList[seqid].sequenceSize)+ " ContactData elements: "+ str(len(messageSequenceList[seqid].datalist)))

                                log_contact_data(seqid)
                                net.processBTReportMessage(str(messageSequenceList[seqid].nodeid))
                                del messageSequenceList[seqid]

                        elif PacketType.network_bat_data == pkttype: #deprecated, will be removed soon
                            batCapacity = float(int.from_bytes(line[cursor:cursor+2], byteorder="big", signed=False)) 
                            cursor+=2
                            batSoC = float(int.from_bytes(line[cursor:cursor+2], byteorder="big", signed=False)) / 10 
                            cursor+=2
                            bytesELT = line[cursor:cursor+2] #ser.read(2)
                            cursor+=2
                            batELT = str(timedelta(minutes=int.from_bytes(bytesELT, byteorder="big", signed=False)))[:-3] # Convert minutes to hours and minutes
                            batAvgConsumption = float(int.from_bytes(line[cursor:cursor+2], byteorder="big", signed=True)) / 10 
                            cursor+=2
                            batAvgVoltage = float(int.from_bytes(line[cursor:cursor+2], byteorder="big", signed=False))/1000 
                            cursor+=2
                            batAvgTemp = float(int.from_bytes(line[cursor:cursor+2], byteorder="big", signed=True)) / 100 
                            cursor+=2
                            net.processBatteryDataMessage(str(nodeid), batAvgVoltage, batCapacity, batSoC, batAvgConsumption, batAvgTemp)

                            if printVerbosity > 1:
                                net.addPrint("  [PACKET DECODE] Battery data, Cap: %.0f mAh SoC: %.1f ETA: %s (hh:mm) Consumption: %.1f mA Voltage: %.3f Temperature %.2f"% (batCapacity, batSoC, batELT, batAvgConsumption, batAvgVoltage, batAvgTemp))


                        elif PacketType.network_respond_ping == pkttype:
                            payload = int(line[cursor:cursor+2].hex(), 16) #int(ser.read(1).hex(), 16)
                            cursor+=2
                            if payload == 233:
                                net.addPrint("  [PACKET DECODE] Node id "+ str(nodeid) +" pinged succesfully!")
                                if nodeid not in ActiveNodes:
                                    ActiveNodes.append(nodeid)
                            else:
                                net.addPrint("  [PACKET DECODE] Node id "+ str(nodeid)+" wrong ping payload: %d" % payload )

                        elif PacketType.network_keep_alive == pkttype:
                            #new_keepalive=True
                           # if new_keepalive:
                            batCapacity = float(int.from_bytes(line[cursor:cursor+2], byteorder="big", signed=False)) 
                            cursor+=2
                            batSoC = float(int.from_bytes(line[cursor:cursor+2], byteorder="big", signed=False)) / 10 
                            cursor+=2
                            bytesELT = line[cursor:cursor+2] #ser.read(2)
                            cursor+=2
                            batELT = str(timedelta(minutes=int.from_bytes(bytesELT, byteorder="big", signed=False)))[:-3] # Convert minutes to hours and minutes
                            batAvgConsumption = float(int.from_bytes(line[cursor:cursor+2], byteorder="big", signed=True)) / 10 
                            cursor+=2
                            batAvgVoltage = float(int.from_bytes(line[cursor:cursor+2], byteorder="big", signed=False))/1000 
                            cursor+=2
                            batAvgTemp = float(int.from_bytes(line[cursor:cursor+2], byteorder="big", signed=True)) / 100 
                            cursor+=2
                            
                            fw_metadata_crc_int = int.from_bytes(line[cursor:cursor+2], byteorder="big", signed=False)
                            cursor+=2
                            fw_metadata_crc_shadow_int = int.from_bytes(line[cursor:cursor+2], byteorder="big", signed=False)
                            cursor+=2
                            fw_metadata_size_int = int.from_bytes(line[cursor:cursor+4], byteorder="big", signed=False)
                            cursor+=4
                            fw_metadata_uuid_int = int.from_bytes(line[cursor:cursor+4], byteorder="big", signed=False)
                            cursor+=4
                            fw_metadata_version_int = int.from_bytes(line[cursor:cursor+2], byteorder="big", signed=False)
                            cursor+=2
                                   
                            fw_metadata_crc_str = "{0}".format(hex(fw_metadata_crc_int))
                            fw_metadata_crc_shadow_str = "{0}".format(hex(fw_metadata_crc_shadow_int))
                            fw_metadata_size_str = "{0}".format(hex(fw_metadata_size_int))
                            fw_metadata_uuid_str = "{0}".format(hex(fw_metadata_uuid_int))
                            fw_metadata_version_str = "{0}".format(hex(fw_metadata_version_int))

                            trickle_count = int.from_bytes(line[cursor:cursor+1], byteorder="big", signed=False) 
                            cursor+=1

                            #net.addPrint("  [PACKET DECODE] Firmware metadata: crc: "+fw_metadata_crc_str+" crc_shadow: "+fw_metadata_crc_shadow_str+" size: "+fw_metadata_size_str+" uuid: "+fw_metadata_uuid_str+" version: "+fw_metadata_version_str)

                            battery_data=dict()
                            battery_data["capacity"]=batCapacity
                            battery_data["state_of_charge"]=batSoC
                            battery_data["estimated_lifetime"]=batELT
                            battery_data["consumption"]=batAvgConsumption
                            battery_data["voltage"]=batAvgVoltage
                            battery_data["temperature"]=batAvgTemp

                            fw_metadata=dict()
                            fw_metadata["crc"]=fw_metadata_crc_int
                            fw_metadata["crc_shadow"]=fw_metadata_crc_shadow_int
                            fw_metadata["size"]=fw_metadata_size_int
                            fw_metadata["uuid"]=fw_metadata_uuid_int
                            fw_metadata["version"]=fw_metadata_version_int
                            
                            nbr_info_str=None
                            if len(line)>cursor:
                                nbr_info=line[cursor:-1]
                                nbr_info_str=nbr_info.decode("utf-8")
                    
                            net.processKeepAliveMessage(str(nodeid), trickle_count, battery_data, fw_metadata, nbr_info_str)

                                #net.processBatteryDataMessage(str(nodeid), batAvgVoltage, batCapacity, batSoC, batAvgConsumption, batAvgTemp)
                                #net.processFWMetadata(str(nodeid), fw_metadata_crc_str, fw_metadata_crc_shadow_str, fw_metadata_size_str, fw_metadata_uuid_str, fw_metadata_version_str)
                            #else:
                            #    batCapacity = float(int.from_bytes(line[cursor:cursor+2], byteorder="big", signed=False)) 
                            #    cursor+=2
                            #    batAvgVoltage = float(int.from_bytes(line[cursor:cursor+2], byteorder="big", signed=False))/1000 
                            #    cursor+=2

                            #    trickle_count = int.from_bytes(line[cursor:cursor+1], byteorder="big", signed=False) 
                            #    cursor+=1

                            #    net.processKeepAliveMessage(str(nodeid), trickle_count, batAvgVoltage, batCapacity)


                            if printVerbosity > 1:
                                net.addPrint("  [PACKET DECODE] Keep alive packet. Cap: "+ str(batCapacity) +" Voltage: "+ str(batAvgVoltage*1000) +" Trickle count: "+ str(trickle_count))

                            #if len(line)>cursor:
                            #    nbr_info=line[cursor:-1]
                            #    net.addPrint("  [PACKET DECODE] nbr info: "+nbr_info.decode("utf-8"))
                                
                        elif PacketType.ota_ack == pkttype:
                            firmwareChunkDownloaded_event_data=line[cursor:]
                            data = int(line[cursor:].hex(), 16) 
                            cursor+=2
                            if printVerbosity > 1:
                                net.addPrint("  [PACKET DECODE] ota_ack received. Data: "+str(data))
                            net.resetNodeTimeout(str(nodeid))
                            firmwareChunkDownloaded_event.set()

                        else:
                            net.addPrint("  [PACKET DECODE] Unknown packet (unrecognized packet type): "+str(rawline))
                            cursor = len(line)

                    else:   #start != START_CHAR
                        startCharErr=True
                        net.addPrint("[UART] Not a packet: "+str(rawline))
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
                ser.reset_input_buffer()
                ser.reset_output_buffer()
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
