# from datetime import timedelta
import paho.mqtt.client as mqtt
from collections import deque
import bt_scheduler

import global_variables as g
from node_class import Node
import params as par
import mqtt_utils

import datetime
import utils
import time

import threading
import traceback
import pexpect
import shutil
import math
import time
import sys
import os

def obtain_and_send_network_status():
    try:
        net_descr=g.net.obtainNetworkDescriptorObject()
        if len(net_descr):
            g.net.addPrint("[EVENT] Network status collected, pushing it to cloud with mqtt.")
            
            node_names=net_descr.keys()
            for n in node_names:
                node_desc={}
                node_desc[n]=net_descr[n]
                mqtt_utils.publish_mqtt(g.mqtt_client, node_desc, par.TELEMETRY_TOPIC) # TODO it stops around 200
                with open("net_stat_store.txt", "a") as f:
                    f.write(datetime.datetime.fromtimestamp(time.time()).strftime('%Y-%m-%d %H:%M:%S')+" "+str(node_desc)+"\n")
    except Exception:
        g.net.addPrint("[EVENT] Error during network status poll")
        pass

class NETWORK_STATUS_POLL_THREAD(threading.Thread):
    def __init__(self, threadID, name):
        threading.Thread.__init__(self)
        self.threadID= threadID
        self.name = name
        self.__loop=True

    def run(self):
        while self.__loop:
            time.sleep(g.network_status_poll_interval)
            obtain_and_send_network_status()

def start_poximity_detection():
    file_to_process=g.filenameContactLog
    utils.close_logs()
    utils.init_logs()
    g.proximity_detector_in_queue.append(file_to_process)

class PROXIMITY_DETECTOR_POLL_THREAD(threading.Thread):
    def __init__(self, threadID, name):
        threading.Thread.__init__(self)
        self.threadID= threadID
        self.name = name
        self.__loop=True

    def run(self):
        while self.__loop:
            time.sleep(par.PROXIMITY_DETECTOR_POLL_INTERVAL)
            start_poximity_detection()
            
class PROXIMITY_DETECTOR_THREAD(threading.Thread):
    def __init__(self, threadID, name):
        threading.Thread.__init__(self)
        self.threadID= threadID
        self.name = name
        self.__loop=True
        self.file_processed=True

    def on_processed(self):
        """ this method is called when the octave script processed the contact log """
        
        g.net.addPrint("[EVENT EXTRACTOR] Contact log file processed!")
        self.file_processed=True
        jdata=utils.get_result()
        try:
            evts=jdata["proximity_events"]
            g.net.addPrint("[EVENT EXTRACTOR] Events: "+str(evts)) #NOTE: At this point the variable evts contains the proximity events. As example I plot the first 10
            if evts!=None: # when events are detected, the last event is appended to a file, while the last events are sent to mqtt 
                t_string=datetime.datetime.fromtimestamp(time.time()).strftime('%Y-%m-%d %H:%M:%S')
                with open("proximity_event_store.txt", "a") as f:
                    f.write(t_string+" "+str(evts)+"\n")
                # mqtt_utils.publish_mqtt(g.mqtt_client, evts, par.TELEMETRY_TOPIC)      
                os.remove(par.OCTAVE_FILES_FOLDER+'/'+ par.EVENTS_FILE_JSON) #remove the file to avoid double processing of it
                evts=None
            else:
                g.net.addPrint("[EVENT EXTRACTOR] No event found!")
        except (KeyError, TypeError):
            g.net.addPrint("[EVENT EXTRACTOR] No event found!")
            return

    def run(self):
        
        octave_process=None

        while self.__loop:
            try:
                if self.file_processed:
                    file_to_process=g.proximity_detector_in_queue.popleft() # TODO sleep and pass if no element in the queue, not through exception

                    self.file_processed=False

                    command=par.OCTAVE_LAUNCH_COMMAND +" "+par.DETECTOR_OCTAVE_SCRIPT+" "+par.octave_to_log_folder_r_path+'/'+file_to_process
                    command = "{} {} {}/{}".format(par.OCTAVE_LAUNCH_COMMAND, par.DETECTOR_OCTAVE_SCRIPT, par.octave_to_log_folder_r_path,file_to_process)
                    g.net.addPrint("[EVENT EXTRACTOR] Sending this command: "+command)
                    octave_process=pexpect.spawnu(command,echo=False,cwd=par.OCTAVE_FILES_FOLDER)

                if par.ENABLE_PROCESS_OUTPUT_ON_CONSOLE:
                    octave_process.expect("\n",timeout=60)
                    octave_return=octave_process.before.split('\r')
                    for s in octave_return:
                        if len(s)>0:
                            g.net.addPrint("[EVENT EXTRACTOR] OCTAVE>>"+s)
                else:
                    octave_process.expect(pexpect.EOF,timeout=5)
                    self.on_processed()

            except pexpect.exceptions.TIMEOUT:
                g.net.addPrint("[EVENT EXTRACTOR] Octave is processing...")
                pass
            except pexpect.exceptions.EOF: # end of octave console, so just after octave execution
                self.on_processed()
                pass
            except IndexError: # if proximity_detector_in_queue is empty
                time.sleep(1)
                pass

class Network(object):

    def __init__(self):
        self.__nodes = []
        threading.Timer(par.NETWORK_PERIODIC_CHECK_INTERVAL,self.__periodicNetworkCheck).start()
        self.__consoleBuffer = deque()
        self.console_queue_lock = threading.Lock()
        self.__lastNetworkPrint=float(time.time())
        self.__netMaxTrickle=0
        self.__netMinTrickle=par.MAX_TRICKLE_C_VALUE
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
        self.__nodes.append(n)
        if len(self.__nodes) == 1:
            self.__expTrickle=n.lastTrickleCount

        #notify thingsboard that a new node has join the network
        connect_data={"device":n.name}
        # mqtt_utils.publish_mqtt(g.mqtt_client, connect_data, par.CONNECT_TOPIC)

    def removeNode(self,n):
        self.__nodes.remove(n)

    def __trickleCheck(self):
        for n in self.__nodes:
            if n==self.__nodes[0]:
                self.__netMaxTrickle = n.lastTrickleCount
                self.__netMinTrickle = n.lastTrickleCount
            if n.lastTrickleCount > self.__netMaxTrickle or ( n.lastTrickleCount == 0 and self.__netMaxTrickle>=(par.MAX_TRICKLE_C_VALUE-1) ):
                self.__netMaxTrickle = n.lastTrickleCount
            if n.lastTrickleCount < self.__netMinTrickle or ( n.lastTrickleCount >= (par.MAX_TRICKLE_C_VALUE-1) and self.__netMinTrickle==0 ):
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
            time.sleep(par.REBOOT_INTERVAL)

    def rebootNode(self,destination_ipv6_addr,reboot_delay_ms):

        breboot_delay_ms = reboot_delay_ms.to_bytes(4, byteorder="big", signed=False)
        payload = breboot_delay_ms

        if destination_ipv6_addr!=None:
            self.sendNewRipple(destination_ipv6_addr,utils.build_outgoing_serial_message(par.PacketType.ota_reboot_node, payload))
        else:
            self.sendNewTrickle(utils.build_outgoing_serial_message(par.PacketType.ota_reboot_node, payload),True)    #by default ingore any ongoing trickle queue and force the reboot
            
    def startFirmwareUpdate(self,autoreboot=True):
        nodes_updated=[]

        node_firmware=None
        try:        
            with open(par.FIRMWARE_FILENAME, "rb") as f:
                node_firmware = f.read()

            if node_firmware!=None:
                self.addPrint("[APPLICATION] Firmware binary: "+par.FIRMWARE_FILENAME+" loaded. Size: "+str(len(node_firmware)))
                metadata_raw=node_firmware[0:par.METADATA_LENGTH]

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
            self.addPrint("[APPLICATION] Exception during firmware file read (check if the file "+par.FIRMWARE_FILENAME+" is there)")
            return

        for n in self.__nodes:
            try:
                if fw_metadata["version"] > n.fw_metadata["version"]:
                    attempts=0
                    destination_ipv6_addr=int(n.name,16)
                    self.addPrint("[APPLICATION] Starting firmware update on node: 0x " +str(n.name))

                    #self.sendFirmware(destination_ipv6_addr,node_firmware)
                    while(self.sendFirmware(destination_ipv6_addr,node_firmware) != 0 and attempts<par.MAX_OTA_ATTEMPTS):
                        self.addPrint("[APPLICATION] Retrying firmware update on node: 0x " +str(n.name))
                        attempts+=1

                    if attempts<par.MAX_OTA_ATTEMPTS:
                        nodes_updated.append(n)
                else:
                    self.addPrint("[APPLICATION] Node: 0x "+str(n.name)+" is already up to date.")
            except KeyError:
                self.addPrint("[APPLICATION] Exception during the ota for Node: 0x "+str(n.name))
                
        if autoreboot:
            self.addPrint("[APPLICATION] Automatically rebooting the nodes that correctly ended the OTA process in: "+str(par.REBOOT_DELAY_S)+"s")
            time.sleep(5) #just wait a bit
            for n in nodes_updated:
                destination_ipv6_addr=int(n.name,16)
                self.rebootNode(destination_ipv6_addr,par.REBOOT_DELAY_S*1000) #Once the reboot command is received by the node, it will start a timer that reboot the node in REBOOT_DELAY
                time.sleep(par.REBOOT_INTERVAL)
            
    def sendFirmware(self, destination_ipv6_addr, firmware):
        EXPECT_ACK_FOR_THE_LAST_PACKET=True #in some cases the CRC check performed with the arrival of the last packet causes the node to crash because of stack overflow. In that case, the last ack won't be received, and if we don't manage this the GW consider the OTA process aborted because of timeout and repeat it.
        #Then when the memory overflow happens we can set the flag EXPECT_ACK_FOR_THE_LAST_PACKET to false so that the OTA process goes on for the rest of the network.


        firmwaresize=len(firmware)
        g.n_chunks_in_firmware=math.ceil(firmwaresize/par.MAX_CHUNK_SIZE)

        offset=0
        chunk_no=1
        chunk_timeout_count=0
        lastCorrectChunkReceived=1 #might it be 0?

        while offset<firmwaresize and chunk_timeout_count<par.MAX_OTA_TIMEOUT_COUNT:
            if offset+par.MAX_CHUNK_SIZE<firmwaresize:
                chunk=firmware[offset:offset+par.MAX_CHUNK_SIZE]
            else:
                chunk=firmware[offset:]

            self.addPrint("[DEBUG] sending firmware chunk number: "+str(chunk_no) + "/"+str(g.n_chunks_in_firmware)+" - "+str((int((offset+par.MAX_CHUNK_SIZE)*10000/firmwaresize))/100)+"%")
            
            chunksize=len(chunk)
            self.sendFirmwareChunk(destination_ipv6_addr,chunk,chunk_no)
            
            self.addPrint("[DEBUG] Chunk sent, waiting ack...")
            
            #wait the response from the remote node
            if chunk_no==1:
                time_to_wait = 10+par.CHUNK_DOWNLOAD_TIMEOUT #the first packet needs more time because the OTA module is initialized
            else:
                time_to_wait = par.CHUNK_DOWNLOAD_TIMEOUT
                            
            if g.firmwareChunkDownloaded_event.wait(time_to_wait):
                g.firmwareChunkDownloaded_event.clear()
                time.sleep(par.CHUNK_INTERVAL_ADD)

                if chunk_no != g.n_chunks_in_firmware: #not the last chunk
                    zero=0
                    chunk_no_b=chunk_no%256
                    ackok=chunk_no_b.to_bytes(1, byteorder="big", signed=False)+zero.to_bytes(1, byteorder="big", signed=True)
                    if g.firmwareChunkDownloaded_event_data==ackok:
                        chunk_timeout_count=0
                        self.addPrint("[DEBUG] OTA ack OK!!") #ok go on
                        lastCorrectChunkReceived=chunk_no
                    else:                   
                        self.addPrint("[DEBUG] OTA ack NOT OK!! Err: "+str(g.firmwareChunkDownloaded_event_data[1])+". The chunk will be retrasmitted in a moment...")
                        time.sleep(5) #in case a full chunk is transmitted to the node twice (because of a ota_ack loss), we might get multiple nacks for a chunk
                        while g.firmwareChunkDownloaded_event.isSet():
                            g.firmwareChunkDownloaded_event.clear()
                            self.addPrint("[DEBUG] More than one ACK/NACK received for this firmware chunk. I should be able to recover...just give me some time")
                            lastCorrectChunkReceived=g.firmwareChunkDownloaded_event_data[0]+256*int(chunk_no/256)
                            time.sleep(5)

                    chunk_no=lastCorrectChunkReceived + 1
                    offset=(chunk_no-1)*par.MAX_CHUNK_SIZE
                else:
                    zero=0
                    one=1
                    chunk_no_b=chunk_no%256
                    ackok=chunk_no_b.to_bytes(1, byteorder="big", signed=False)+zero.to_bytes(1, byteorder="big", signed=True) #everithing fine on the node
                    ackok_alt=chunk_no_b.to_bytes(1, byteorder="big", signed=False)+one.to_bytes(1, byteorder="big", signed=True) #crc ok, but there was memory overflow during CRC verification. The firmware will be vefiried on the next reboot
                    if g.firmwareChunkDownloaded_event_data==ackok:
                        chunk_no+=1
                        offset=(chunk_no-1)*par.MAX_CHUNK_SIZE
                        return 0 #OTA update correctly closed
                    elif g.firmwareChunkDownloaded_event_data==ackok_alt:
                        self.addPrint("[OTA] CRC ok but not written on the node...")
                        chunk_no+=1
                        offset=(chunk_no-1)*par.MAX_CHUNK_SIZE
                        return 0 #OTA update correctly closed even if a memory overflow happened on the node during CRC verification. The firmware will be vefiried on the next reboot
                    else:
                        self.addPrint("[OTA] CRC error at the node...")
                        return 1 #crc error at the node
            else:
                if chunk_no == g.n_chunks_in_firmware and not EXPECT_ACK_FOR_THE_LAST_PACKET: #the last packet
                    return 0 #OTA update correctly closed
                chunk_timeout_count+=1
                self.addPrint("[DEBUG] Chunk number "+str(chunk_no)+" ack timeout, retry!")

    def sendFirmwareChunk(self, destination_ipv6_addr, chunk,chunk_no):
        chunksize=len(chunk)
        g.no_of_sub_chunks_in_this_chunk=math.ceil(chunksize/par.MAX_SUBCHUNK_SIZE)
        
        sub_chunk_no=0
        offset=0
        while offset<chunksize:
            sub_chunk_no+=1
            if offset+par.MAX_SUBCHUNK_SIZE < chunksize:
                sub_chunk=chunk[offset:offset+par.MAX_SUBCHUNK_SIZE]
                offset=offset+par.MAX_SUBCHUNK_SIZE
            else:
                sub_chunk=chunk[offset:]
                offset=chunksize
            self.sendFirmwareSubChunk(destination_ipv6_addr, sub_chunk,chunk_no,sub_chunk_no)
            if sub_chunk_no != g.no_of_sub_chunks_in_this_chunk:
                time.sleep(par.SUBCHUNK_INTERVAL)

    def sendFirmwareSubChunk(self, destination_ipv6_addr, sub_chunk, chunk_no, sub_chunk_no):        
        if chunk_no==1 and sub_chunk_no==1:
            packetType=par.PacketType.ota_start_of_firmware   #start of firmware
            self.addPrint("[DEBUG] PacketType start of firmware")
            self.addPrint("[DEBUG] Subchunk size: " +str(len(sub_chunk)))
           
        elif chunk_no==g.n_chunks_in_firmware and sub_chunk_no==g.no_of_sub_chunks_in_this_chunk:
            packetType=par.PacketType.ota_end_of_firmware   #end of firmware
            self.addPrint("[DEBUG] PacketType end of firmware")
        else:
            packetType=par.PacketType.ota_more_of_firmware   #all other packets

        chunk_no_8b=chunk_no%256
        sub_chunk_no_8b=sub_chunk_no%256

        data=chunk_no_8b.to_bytes(1, byteorder="big", signed=False)+sub_chunk_no_8b.to_bytes(1, byteorder="big", signed=False)+sub_chunk
        
        self.sendNewRipple(destination_ipv6_addr,utils.build_outgoing_serial_message(packetType, data))

    def sendNewRipple(self, destination_addr, message):
        destination_addr_bytes=destination_addr.to_bytes(length=16, byteorder='big', signed=False)

        destination_addr_ascii=''.join('%02X'%i for i in destination_addr_bytes)
        destination_addr_ascii_encoded=destination_addr_ascii.encode('utf-8')
        message=destination_addr_ascii_encoded+message

        utils.send_serial_msg(message)

    def sendNewTrickle(self, message,forced=False):
        broadcast_ipv6_addr=0xFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF
        destination_addr_bytes=broadcast_ipv6_addr.to_bytes(length=16, byteorder='big', signed=False)

        destination_addr_ascii=''.join('%02X'%i for i in destination_addr_bytes)
        destination_addr_ascii_encoded=destination_addr_ascii.encode('utf-8')
        message=destination_addr_ascii_encoded+message

        if forced or destination_addr_bytes==broadcast_ipv6_addr:
            self.__trickleQueue.clear()
            utils.send_serial_msg(message)
            g.net.addPrint("[APPLICATION] Trickle message: 0x {0} force send".format((message).hex()))
        else:
            if self.__trickleCheck():
                self.__expTrickle = (self.__netMaxTrickle + 1)%par.MAX_TRICKLE_C_VALUE
                utils.send_serial_msg(message)
                g.net.addPrint("[APPLICATION] Trickle message: 0x {0} sent".format((message).hex()))
            else:
                self.__trickleQueue.append(message)
                self.addPrint("[APPLICATION] Trickle message: 0x {0} queued".format((message).hex()))

    def __periodicNetworkCheck(self):
        threading.Timer(par.NETWORK_PERIODIC_CHECK_INTERVAL,self.__periodicNetworkCheck).start()
        nodes_removed = False
        for n in self.__nodes:
            if n.getLastMessageElapsedTime() > par.NODE_TIMEOUT_S and n.online:
                if par.printVerbosity > 2:
                    self.addPrint("[APPLICATION] Node "+ n.name +" timed out. Elasped time: %.2f" %n.getLastMessageElapsedTime() +" Removing it from the network.")
                # self.removeNode(n)
                n.online=False                
                nodes_removed = True

        #self.__trickleCheck()

        if nodes_removed:
            self.__trickleCheck()
            self.printNetworkStatus()

    def obtainNetworkDescriptorObject(self):
        net_descr=dict()
        for n in self.__nodes:
            if n.online: #do not publish outdated data
                n_descr=n.getNodeDescriptorObject()
                n_descr_flat=utils.flatten_json(n_descr)
                net_descr[n.name]=[{'ts':int(time.time()*1000), 'values':n_descr_flat}]

        return net_descr

    def printNetworkStatus(self):

        if(float(time.time()) - self.__lastNetworkPrint < 0.2): #this is to avoid too fast call to this function
            return

        __lastNetworkPrint = float(time.time())

        netSize = len(self.__nodes)

        if par.CLEAR_CONSOLE:
            utils.cls()

        # Drawing the console
        utils.console_header(netSize, self.__uartLogLines, self.__netMinTrickle, self.__netMaxTrickle, self.__expTrickle, self.__trickleQueue)
        for n in self.__nodes:
            n.printNodeInfo()
        utils.console_middle(self.showHelp)
        # end of console main header
        
        terminalSize = shutil.get_terminal_size(([80,20]))
        if self.showHelp:
            availableLines = terminalSize[1] - (24 + len(self.__nodes))
        else:
            availableLines = terminalSize[1] - (12 + len(self.__nodes))

        while( (len(self.__consoleBuffer) > availableLines) and self.__consoleBuffer):
            with self.console_queue_lock:
                self.__consoleBuffer.popleft()

        with self.console_queue_lock:
            for l in self.__consoleBuffer:
                print(l)

        if not par.CLEAR_CONSOLE:
            empty_lines_no=availableLines-len(self.__consoleBuffer)-1
            if(empty_lines_no>0):
                empty_lines='\n'*empty_lines_no
                print(empty_lines)

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
                self.__expTrickle = (self.__netMaxTrickle + 1)%par.MAX_TRICKLE_C_VALUE
                message=self.__trickleQueue.popleft()
                utils.send_serial_msg(message)
                self.addPrint("[APPLICATION] Trickle message: 0x {0} automatically sent".format((message).hex()))
        else:
            self.__trickleCheck()
        self.printNetworkStatus()

    def processBTReportMessage(self, label):
        """ this method keeps track of the amount of bluetooth packet recieved for each node """
        n = self.getNode(label)
        if n != None:
            n.BTReportHandler()

    def processUARTError(self):
        self.__uartLogLines=self.__uartLogLines+1

    def resetNodeTimeout(self, label):
        n = self.getNode(label)
        if n != None:
            n.resetNodeTimeout()

    def addPrint(self, text):
        g.consoleLogger.info(text)
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

    def resetCounters(self):
        for n in self.__nodes:
            n.amountOfBTReports=0
        self.__trickleCheck()

    def resetTrickle(self):
        self.__trickleCheck()
        self.__expTrickle = self.__netMaxTrickle
        
class USER_INPUT_THREAD(threading.Thread):
    def __init__(self, threadID, name):
        threading.Thread.__init__(self)
        self.threadID= threadID
        self.name = name
        self.__loop=True

    def run(self):
        while self.__loop:
            try:
                ble_tof_enabled=False
                input_str = input()
                if len(input_str)>=2 and input_str[0] == 'f':
                    forced=True
                    user_input = int(input_str[1:])
                else:
                    if input_str=='h':
                        if g.net.showHelp:
                            g.net.showHelp=False
                        else:
                            g.net.showHelp=True

                        g.net.printNetworkStatus()
                        continue
                    elif input_str=='r':
                        g.net.resetCounters()
                        g.net.printNetworkStatus()
                        continue
                    elif input_str=='t':
                        g.net.resetTrickle()
                        g.net.printNetworkStatus()
                        continue
                    elif input_str=='f':
                        g.net.addPrint("[USER_INPUT] Firmware update start.")
                        g.net.startFirmwareUpdate()
                        continue
                    elif input_str=='b':
                        g.net.addPrint("[USER_INPUT] Network reboot requested!")
                        g.net.rebootNetwork(5000)
                        continue
                    elif input_str=='p':
                        g.net.addPrint("[USER_INPUT] Process contacts log!")
                        start_poximity_detection()
                        obtain_and_send_network_status()
                        continue
                    else:
                        forced=False
                        user_input = int(input_str)

                if user_input == 1:
                    payload = 233
                    g.net.addPrint("[USER_INPUT] Ping request")
                    g.net.sendNewTrickle(utils.build_outgoing_serial_message(par.PacketType.network_request_ping, payload.to_bytes(1, byteorder="big", signed=False)),forced)
                elif user_input == 2:
                    g.net.addPrint("[USER_INPUT] Turn bluetooth on")
                    g.net.sendNewTrickle(utils.build_outgoing_serial_message(par.PacketType.nordic_turn_bt_on, None),forced)
                elif user_input == 3:
                    g.net.addPrint("[USER_INPUT] Turn bluetooth off")
                    g.net.sendNewTrickle(utils.build_outgoing_serial_message(par.PacketType.nordic_turn_bt_off, None),forced)
                elif user_input == 4:
                    if ble_tof_enabled:
                        g.net.addPrint("[USER_INPUT] disabling ble tof, WARNING: never really tested in vela")
                        ble_tof_enabled=False
                    else:
                        g.net.addPrint("[USER_INPUT] enabling ble tof, WARNING: never really tested in vela")
                        ble_tof_enabled=True
                    
                    g.net.sendNewTrickle(utils.build_outgoing_serial_message(par.PacketType.nordic_ble_tof_enable, ble_tof_enabled.to_bytes(1, byteorder="big", signed=False)),forced)
                        
                elif user_input == 5:
                    # TODO pack everython in a util function, as soon as you can test this code
                    active_scan = 1
                    scan_interval = int(par.SCAN_INTERVAL_MS*1000/625)
                    scan_window = int(par.SCAN_WINDOW_MS*1000/625)
                    timeout = int(par.SCAN_TIMEOUT_S)
                    report_timeout_ms = int(par.REPORT_TIMEOUT_S*1000)
                    bactive_scan = active_scan.to_bytes(1, byteorder="big", signed=False)
                    bscan_interval = scan_interval.to_bytes(2, byteorder="big", signed=False)
                    bscan_window = scan_window.to_bytes(2, byteorder="big", signed=False)
                    btimeout = timeout.to_bytes(2, byteorder="big", signed=False)
                    breport_timeout_ms = report_timeout_ms.to_bytes(4, byteorder="big", signed=False)
                    payload = bactive_scan + bscan_interval + bscan_window + btimeout + breport_timeout_ms
                    g.net.addPrint("[USER_INPUT] Turn bluetooth on with parameters: scan_int="+str(par.SCAN_INTERVAL_MS)+"ms, scan_win="+str(par.SCAN_WINDOW_MS)+"ms, timeout="+str(par.SCAN_TIMEOUT_S)+"s, report_int="+str(par.REPORT_TIMEOUT_S)+"s")
                    g.net.sendNewTrickle(utils.build_outgoing_serial_message(par.PacketType.nordic_turn_bt_on_w_params, payload),forced)
                elif user_input == 8:
                    g.net.addPrint("[USER_INPUT] Reset nordic")
                    g.net.sendNewTrickle(utils.build_outgoing_serial_message(par.PacketType.nordic_reset, None),forced)
                elif user_input == 9:
                    time_between_send_ms = 1500
                    time_between_send = time_between_send_ms.to_bytes(2, byteorder="big", signed=False)
                    g.net.addPrint("[USER_INPUT] Set time between sends to "+ str(time_between_send_ms) + "ms")
                    g.net.sendNewTrickle(utils.build_outgoing_serial_message(par.PacketType.network_set_time_between_send, time_between_send),forced)
                elif user_input > 9:
                    interval = user_input
                    g.net.addPrint("[USER_INPUT] Set keep alive interval to "+ str(interval) + "s")
                    g.net.sendNewTrickle(utils.build_outgoing_serial_message(par.PacketType.ti_set_keep_alive, interval.to_bytes(1, byteorder="big", signed=False)),forced)
                    g.network_status_poll_interval=interval
            except Exception as e:
                g.net.addPrint("[USER_INPUT] Read failed. Read data: "+ input_str)
                traceback.print_exc()
                pass

if __name__ == "__main__":
  
  utils.create_log_folder()
  utils.init_logs()
  utils.init_serial()

  g.net = Network()
  g.net.addPrint("[APPLICATION] Starting...")
  while g.mqtt_client==None:
    g.mqtt_client=mqtt_utils.connect_mqtt()
  g.mqtt_client.loop_start()

  user_input_thread=USER_INPUT_THREAD(4,"user input thread")
  user_input_thread.setDaemon(True)
  user_input_thread.start()

  proximity_detector_thread=PROXIMITY_DETECTOR_THREAD(5,"proximity detection process")
  proximity_detector_thread.setDaemon(True)
  proximity_detector_thread.start()

  proximity_detec_poll_thread=PROXIMITY_DETECTOR_POLL_THREAD(6,"periodic proximity detection trigger process")
  proximity_detec_poll_thread.setDaemon(True)
  proximity_detec_poll_thread.start()

  network_stat_poll_thread=NETWORK_STATUS_POLL_THREAD(7,"periodic network status monitor")
  network_stat_poll_thread.setDaemon(True)
  network_stat_poll_thread.start()

  bluetooth_scheduling_thread=bt_scheduler.BLUETOOTH_SCHEDULING_THREAD(7,"bluetooth scheduling")
  bluetooth_scheduling_thread.setDaemon(True)
  bluetooth_scheduling_thread.start()

  if g.ser.is_open:
      g.net.addPrint("[UART] Serial Port already open! "+ g.ser.port + " open before initialization... closing first")
      g.ser.close()
      time.sleep(1)

  startCharErr=False
  otherkindofErr=False
  previousTimeTimeout = 0
  try:
      while 1:
          if g.ser.is_open:
              try:
                  bytesWaiting = g.ser.in_waiting
              except Exception as e:
                  g.net.addPrint("[UART] Serial Port input exception:"+ str(e))
                  bytesWaiting = 0
                  g.ser.close()
                  time.sleep(1)
                  continue
              try:

                  if bytesWaiting > 0:
                      rawline = g.ser.readline() #can block if no timeout is provided when the port is open
                      g.UARTLogger.info('[SINK] ' + str(rawline))
                      start = rawline[0:1].hex()

                      if start == par.START_CHAR:
                          if startCharErr or otherkindofErr: # if the reading of the previous uart stream went wrong
                              g.net.processUARTError()         # just go to the next line
                              startCharErr=False
                              otherkindofErr=False

                          line = utils.to_byte(rawline[1:-1])
                          g.deliverCounter += 1
                          cursor, nodeid, pkttype, pktnum = utils.decode_node_info(line)

                          if par.printVerbosity > 0:
                              g.net.addPrint("[NODEID " + str(nodeid) + "] pkttype " + hex(pkttype) + ", pktnum " + str(pktnum))

                          utils.check_for_packetdrop(nodeid, pktnum)

                          #TODO: add here the check for duplicates. Any duplicate should be discarded HERE!!!

                          # Here starts the sequence of if-else to check the type of packet recieved 
                          if par.PacketType.network_new_sequence == pkttype:
                              datalen = int(line[cursor:cursor+2].hex(), 16)                              
                              cursor+=2
                              payload = line[cursor:].hex()
                              cursor = len(line) # TODO try to remove this line, looks useless
                              if datalen % par.SINGLE_NODE_REPORT_SIZE != 0:
                                  if par.printVerbosity > 1:
                                      g.net.addPrint("  [PACKET DECODE] Invalid datalength: "+ str(datalen))
                              seqid = utils.get_sequence_index(nodeid)

                              #at this point, since we just received 'network_new_sequence', there should not be any sequence associated with this nodeid
                              if seqid != -1: #if this is true something went wrong (duplicate or packet lost from previous report)
                                  if g.messageSequenceList[seqid].lastPktnum == pktnum: #duplicate
                                      if par.printVerbosity > 1:
                                          g.net.addPrint("  [PACKET DECODE] Duplicate packet from node "+ str(nodeid)+ " with pktnum "+ str(pktnum))
                                      continue
                                  else:   #'network_new_sequence' arrived before 'network_last_sequence'. Probably the last packet of the previous report has been lost
                                      if par.printVerbosity > 1:
                                          g.net.addPrint("  [PACKET DECODE] Previous sequence has not been completed yet")
                                      # TODO what to do now? For now, assume last packet was dropped
                                      utils.log_contact_data(g.messageSequenceList[seqid])                         # store received data instead of deleting it all
                                      del g.messageSequenceList[seqid]                  # remove data already stored, seqid comes from the get sequence index function

                              g.messageSequenceList.append(par.MessageSequence(int(time.time()*1000), nodeid, pktnum, 0, 0, [], time.time()))
                              seqid = len(g.messageSequenceList) - 1

                              g.messageSequenceList[seqid].sequenceSize += datalen

                              if g.messageSequenceList[seqid].sequenceSize > par.MAX_PACKET_PAYLOAD: # There will be a following packet
                                  utils.decode_payload(payload, seqid, par.MAX_PACKET_PAYLOAD)
                              else:   #this is when the sequence is made by only one packet
                                  utils.decode_payload(payload, seqid, datalen)

                                  if par.printVerbosity > 1:
                                      g.net.addPrint("  [PACKET DECODE] Bluetooth sequence decoded. Contact elements: "+ str(len(g.messageSequenceList[seqid].datalist)))
                                  utils.log_contact_data(g.messageSequenceList[seqid])
                                  g.net.processBTReportMessage(str(nodeid))

                                  del g.messageSequenceList[seqid]

                          elif par.PacketType.network_active_sequence == pkttype:
                              seqid = utils.get_sequence_index(nodeid)
                              payload = line[cursor:].hex()
                              cursor = len(line)
                              if seqid == -1:             #if this is true something went wrong. Maybe we have lost the network_new_sequence packet of this report. It means we received 'network_active_sequence' before receiving a valid 'network_new_sequence'
                                  if par.printVerbosity > 1:
                                      g.net.addPrint("  [PACKET DECODE] First part of sequence dropped, creating incomplete sequence at index "+ str(len(g.messageSequenceList)) +" from pktnum "+ str(pktnum))
                                  g.messageSequenceList.append( par.MessageSequence(int(time.time()*1000), nodeid, pktnum, -1, 0, [], time.time()))
                                  seqid = len(g.messageSequenceList) - 1
                                  g.headerDropCounter += 1

                              elif g.messageSequenceList[seqid].lastPktnum == pktnum: #duplicate
                                  if par.printVerbosity > 1:
                                      g.net.addPrint("  [PACKET DECODE] Duplicate packet from node "+ str(nodeid) +" with pktnum "+ str(pktnum))
                                  continue

                              g.messageSequenceList[seqid].lastPktnum = pktnum
                              g.messageSequenceList[seqid].latestTime = time.time()
                              utils.decode_payload(payload,seqid, par.MAX_PACKET_PAYLOAD)

                          elif par.PacketType.network_last_sequence == pkttype:
                              seqid = utils.get_sequence_index(nodeid)
                              payload = line[cursor:].hex()
                              cursor = len(line)
                              if seqid == -1:             #if this is true something went wrong. Maybe we just received network_last_sequence for this report. It means we received 'network_last_sequence' before receiving a valid 'network_new_sequence'
                                  g.messageSequenceList.append(par.MessageSequence(int(time.time()*1000), nodeid, pktnum, 0, 0, [], time.time()))
                                  seqid = len(g.messageSequenceList) - 1

                                  utils.decode_payload(payload,seqid, par.MAX_PACKET_PAYLOAD)
                                  if par.printVerbosity > 1:
                                      g.net.addPrint("  [PACKET DECODE] Bluetooth sequence decoded but header files were never received.  datacounter: "+ str(g.messageSequenceList[seqid].datacounter) +" ContactData elements: "+ str(len(g.messageSequenceList[seqid].datalist)))

                                  g.headerDropCounter += 1
                                  utils.log_contact_data(g.messageSequenceList[seqid])
                                  g.net.processBTReportMessage(str(g.messageSequenceList[seqid].nodeid))
                                  del g.messageSequenceList[seqid]

                              elif g.messageSequenceList[seqid].sequenceSize == -1: # if we have lost network_new_sequence, but at least one network_active_sequence is correctly received, sequense size is initialized to -1 (the real value was in network_new_sequence, therefore we lost it)
                                  utils.decode_payload(payload,seqid, par.MAX_PACKET_PAYLOAD)
                                  if par.printVerbosity > 1:
                                      g.net.addPrint("  [PACKET DECODE] Bluetooth sequence decoded but header files were never received.  datacounter: "+ str(g.messageSequenceList[seqid].datacounter) +" ContactData elements: "+ str(len(g.messageSequenceList[seqid].datalist)))
                                  utils.log_contact_data(g.messageSequenceList[seqid])
                                  g.net.processBTReportMessage(str(g.messageSequenceList[seqid].nodeid))
                                  del g.messageSequenceList[seqid]

                              else:                       # normal behaviour
                                  remainingDataSize = g.messageSequenceList[seqid].sequenceSize - g.messageSequenceList[seqid].datacounter

                                  #TODO: remainingDataSize now should be equal to the size of this payload, instead of using inequalities use straigt ==. But carefully check the cases and sizes
                                  if remainingDataSize > par.MAX_PACKET_PAYLOAD: #the value in sequenceSize is probably not correct. Did we lost a lot of packets?
                                      utils.decode_payload(payload,seqid, par.MAX_PACKET_PAYLOAD)
                                  else:                   # normal behaviour
                                      utils.decode_payload(payload,seqid, remainingDataSize)

                                  if g.messageSequenceList[seqid].sequenceSize != g.messageSequenceList[seqid].datacounter:
                                      if par.printVerbosity > 1:
                                          g.net.addPrint("  [PACKET DECODE] ERROR: Messagesequence ended, but datacounter ({}) is not equal to sequencesize ({})".format(g.messageSequenceList[seqid].datacounter,g.messageSequenceList[seqid].sequenceSize ))

                                  if par.printVerbosity > 1:
                                      g.net.addPrint("  [PACKET DECODE] Bluetooth sequence decoded. "+" sequencesize: "+ str(g.messageSequenceList[seqid].sequenceSize)+ " ContactData elements: "+ str(len(g.messageSequenceList[seqid].datalist)))

                                  utils.log_contact_data(g.messageSequenceList[seqid])
                                  g.net.processBTReportMessage(str(g.messageSequenceList[seqid].nodeid))
                                  del g.messageSequenceList[seqid]

                          elif par.PacketType.network_respond_ping == pkttype:
                              payload = int(line[cursor:cursor+2].hex(), 16) #int(ser.read(1).hex(), 16)
                              cursor+=2
                              if payload == 233:
                                  g.net.addPrint("  [PACKET DECODE] Node id "+ str(nodeid) +" pinged succesfully!")
                              else:
                                  g.net.addPrint("  [PACKET DECODE] Node id "+ str(nodeid)+" wrong ping payload: %d" % payload )

                          elif par.PacketType.network_keep_alive == pkttype:
                              cursor, trickle_count, battery_data, fw_metadata = utils.keepalive_decode(line, cursor)
                              
                              nbr_info_str=None
                              if len(line)>cursor:
                                  nbr_info=line[cursor:-1]
                                  nbr_info_str=nbr_info.decode("utf-8")
                      
                              g.net.processKeepAliveMessage(str(nodeid), trickle_count, battery_data, fw_metadata, nbr_info_str)

                              if par.printVerbosity > 1:
                                  g.net.addPrint("  [PACKET DECODE] Keep alive packet. Cap: "+ str(battery_data["capacity"]) +" Voltage: "+ str(battery_data["voltage"]*1000) +" Trickle count: "+ str(trickle_count))

                          elif par.PacketType.ota_ack == pkttype:
                              g.firmwareChunkDownloaded_event_data=line[cursor:]
                              data = int(line[cursor:].hex(), 16) 
                              cursor+=2
                              if par.printVerbosity > 1:
                                  g.net.addPrint("  [PACKET DECODE] ota_ack received. Data: "+str(data))
                              g.net.resetNodeTimeout(str(nodeid))
                              g.firmwareChunkDownloaded_event.set()

                          else:
                              g.net.addPrint("  [PACKET DECODE] Unknown packet (unrecognized packet type): "+str(rawline))
                              cursor = len(line)

                      else:   #start != START_CHAR
                          startCharErr=True
                          g.errorLogger.info("%s" %(str(rawline)))
                          if par.printVerbosity > 2:
                              g.net.addPrint("[UART] Not a packet: "+str(rawline))
                  else:   #in_waiting==0
                      time.sleep(0.1)

              except Exception as e:
                  otherkindofErr=True
                  g.net.addPrint("[ERROR] Unknown error during line decoding. Exception: %s. Line was: %s" %( str(e), str(rawline)))
                  g.errorLogger.info("%s" %(str(rawline)))

              currentTime = time.time()
              if currentTime - previousTimeTimeout > par.TIMEOUT_INTERVAL:
                  previousTimeTimeout = time.time()
                  deletedCounter = 0
                  for x in range(len(g.messageSequenceList)):
                      if currentTime - g.messageSequenceList[x - deletedCounter].latestTime > par.TIMEOUT_TIME:
                          deleted_nodeid=g.messageSequenceList[x - deletedCounter].nodeid
                          del g.messageSequenceList[x - deletedCounter]
                          deletedCounter += 1
                          if par.printVerbosity > 1:
                              xd = x + deletedCounter
                              g.net.addPrint("[APPLICATION] Deleted seqid %d of node %d because of timeout" %(xd, deleted_nodeid))


          else: # !ser.is_open (serial port is not open)

              g.net.addPrint('[UART] Serial Port closed! Trying to open port: %s'% g.ser.port)

              try:
                  g.ser.open()
                  g.ser.reset_input_buffer()
                  g.ser.reset_output_buffer()
              except Exception as e:
                  g.net.addPrint("[UART] Serial Port open exception:"+ str(e))
                  time.sleep(5)
                  continue

              g.net.addPrint("[UART] Serial Port open!")


  except UnicodeDecodeError as e:
      pass

  except KeyboardInterrupt:
      print("[APPLICATION] -----Packet delivery stats summary-----")
      print("[APPLICATION] Total packets delivered: ", g.deliverCounter)
      print("[APPLICATION] Total packets dropped: ", g.dropCounter)
      print("[APPLICATION] Total header packets dropped: ", g.headerDropCounter)
      print("[APPLICATION] Packet delivery rate: ", 100 * (g.deliverCounter / (g.deliverCounter + g.dropCounter))) # TODO bassissimo
      raise
  finally:
      print("done")
