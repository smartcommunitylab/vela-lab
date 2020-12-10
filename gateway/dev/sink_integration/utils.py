from itertools import chain, starmap
import global_variables as g
import params as par
import binascii
import datetime
import logging
import shutil
import serial
import time
import json
import os


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

def decode_node_info(line):
  # TODO change the way pktnum's are being tracked
  cursor = 0
  source_addr_bytes=line[cursor:cursor+16]
  nodeid_int = int.from_bytes(source_addr_bytes, "big", signed=False) 
  nodeid='%(nodeid_int)X'%{"nodeid_int": nodeid_int}
  cursor+=16
  pkttype = int(line[cursor:cursor+2].hex(),16)
  cursor+=2
  pktnum = int.from_bytes(line[cursor:cursor+1], "little", signed=False) 
  cursor+=1
  return cursor, nodeid, pkttype, pktnum

""" check the number of dropped packages during the whole life of the network """
def check_for_packetdrop(nodeid, pktnum):
    if pktnum == 0:
        return
    for x in range(len(g.nodeDropInfoList)):
        if g.nodeDropInfoList[x].nodeid == nodeid:
            g.dropCounter += pktnum - g.nodeDropInfoList[x].lastpkt - 1
            return
    g.nodeDropInfoList.append(par.NodeDropInfo(nodeid, pktnum))

def get_sequence_index(nodeid):
    for x in range(len(g.messageSequenceList)):
        if g.messageSequenceList[x].nodeid == nodeid:
            return x
    return -1

def decode_payload(payload, seqid, size, packettype): #TODO: packettype can be removed
    cur=0
    try:
        for x in range(round(size / par.SINGLE_NODE_REPORT_SIZE)):
            nid = int(payload[cur:cur+12], 16) #int(ser.read(6).hex(), 16)
            cur=cur+12
            lastrssi = int(payload[cur:cur+2], 16) #int(ser.read(1).hex(), 16)
            cur=cur+2
            maxrssi = int(payload[cur:cur+2], 16) #int(ser.read(1).hex(), 16)
            cur=cur+2
            pktcounter = int(payload[cur:cur+2], 16) #int(ser.read(1).hex(), 16)
            cur=cur+2
            #net.addPrint("id = {:012X}".format(nid, 8))
            contact = par.ContactData(nid, lastrssi, maxrssi, pktcounter)
            g.messageSequenceList[seqid].datalist.append(contact)
            g.messageSequenceList[seqid].datacounter += par.SINGLE_NODE_REPORT_SIZE
    except ValueError:
        g.net.addPrint("[Node {0}] Requested to decode more bytes than available. Requested: {1}"
                          .format(g.messageSequenceList[seqid].nodeid, size))
    finally:
        #dataLogger.info(raw_data)
        return

def create_log_folder():
  if not os.path.exists(par.LOG_FOLDER_PATH):
      try:
          os.mkdir(par.LOG_FOLDER_PATH)
          print("Log directory not found.")
          print("%s Created " % par.LOG_FOLDER_PATH)
      except Exception as e:
          print("Can't get the access to the log folder %s." % par.LOG_FOLDER_PATH)
          print("Exception: %s" % str(e))
      time.sleep(2)
  return

def init_serial():
  g.ser = serial.Serial()
  g.ser.port = par.SERIAL_PORT
  g.ser.baudrate = par.BAUD_RATE
  g.ser.timeout = par.TIMEOUT
  #g.ser.inter_byte_timeout = 0.1 #not the space between stop/start condition
  return
""" initialize various loggers """
def init_logs():
  formatter = logging.Formatter('%(asctime)s %(levelname)s %(message)s', datefmt='%Y/%m/%d %H:%M:%S')
  timestr = time.strftime("%Y%m%d-%H%M%S")

  nameConsoleLog = "consoleLogger"
  filenameConsoleLog = timestr + "-console.log"
  fullpathConsoleLog = par.LOG_FOLDER_PATH + filenameConsoleLog

  g.consolelog_handler = logging.FileHandler(fullpathConsoleLog)
  g.consolelog_handler.setFormatter(formatter)
  g.consoleLogger = logging.getLogger(nameConsoleLog)
  g.consoleLogger.setLevel(par.LOG_LEVEL)
  g.consoleLogger.addHandler(g.consolelog_handler)

  nameUARTLog = "UARTLogger"
  filenameUARTLog = timestr + "-UART.log"
  fullpathUARTLog = par.LOG_FOLDER_PATH+filenameUARTLog
  g.uartlog_handler = logging.FileHandler(fullpathUARTLog)
  g.uartlog_handler.setFormatter(formatter)
  g.UARTLogger = logging.getLogger(nameUARTLog)
  g.UARTLogger.setLevel(par.LOG_LEVEL)
  g.UARTLogger.addHandler(g.uartlog_handler)

  nameContactLog = "contactLogger"
  g.filenameContactLog = timestr + "-contact.log" # global variable for proximity detection function
  fullpathContactLog = par.LOG_FOLDER_PATH+g.filenameContactLog
  contact_formatter = logging.Formatter('%(message)s')

  g.contactlog_handler = logging.FileHandler(fullpathContactLog)
  g.contactlog_handler.setFormatter(contact_formatter)
  g.contactLogger = logging.getLogger(nameContactLog)
  g.contactLogger.setLevel(par.LOG_LEVEL)
  g.contactLogger.addHandler(g.contactlog_handler)

  nameErrorLog = "errorLogger"
  filenameErrorLog = timestr + "-errorLogger.log"
  fullpathErrorLog = par.LOG_FOLDER_PATH+filenameErrorLog
  errorlog_formatter= logging.Formatter('%(message)s')

  g.errorlog_handler = logging.FileHandler(fullpathErrorLog)
  g.errorlog_handler.setFormatter(errorlog_formatter)
  g.errorLogger = logging.getLogger(nameErrorLog)
  g.errorLogger.setLevel(par.LOG_LEVEL)
  g.errorLogger.addHandler(g.errorlog_handler)
  return

def close_logs():
  logging.shutdown()
  g.consoleLogger.removeHandler(g.consolelog_handler)
  g.UARTLogger.removeHandler(g.uartlog_handler)
  g.contactLogger.removeHandler(g.contactlog_handler)
  g.errorLogger.removeHandler(g.errorlog_handler)
  return

def to_byte(hex_text):
    return binascii.unhexlify(hex_text)

def cls():
    """ to clear the screen, used by the console """
    os.system('cls' if os.name=='nt' else 'clear')

def get_result():  
    """ open the JSON file with the proximity events and load it """
    try:
        with open(par.OCTAVE_FILES_FOLDER+'/'+par.EVENTS_FILE_JSON) as f: # Use file to refer to the file object
            data=f.read()
            return json.loads(data)
    except OSError:
        return None
        
    return None

def keepalive_decode(line,cursor):
  batCapacity = float(int.from_bytes(line[cursor:cursor+2], byteorder="big", signed=False)) 
  cursor+=2
  batSoC = float(int.from_bytes(line[cursor:cursor+2], byteorder="big", signed=False)) / 10 
  cursor+=2
  bytesELT = line[cursor:cursor+2] 
  cursor+=2
  batELT = str(datetime.timedelta(minutes=int.from_bytes(bytesELT, byteorder="big", signed=False)))[:-3] # Convert minutes to hours and minutes
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

  trickle_count = int.from_bytes(line[cursor:cursor+1], byteorder="big", signed=False) 
  cursor+=1

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

  return cursor, trickle_count, battery_data, fw_metadata

def console_header(netSize, uartLogLines, netMinTrickle, netMaxTrickle, expTrickle, trickleQueue):
  print("|----------------------------------------------------------------------------------------------------------------------------------------------|")
  print("|---------------------------------------------------|  Network size %3s log lines %7s |----------------------------------------------------|" %(str(netSize), uartLogLines))
  print("|------------------------------------------| Trickle: min %3d; max %3d; exp %3d; queue size %2d |-----------------------------------------------|" %(netMinTrickle, netMaxTrickle, expTrickle, len(trickleQueue)))
  print("|----------------------------------------------------------------------------------------------------------------------------------------------|")
  print("| NodeID | Battery                              | Last    | Firmware | Trick |   #BT  |                                  Neighbors info string |")
  print("|        | Volt   SoC Capacty    Cons    Temp   | seen[s] | version  | Count |   Rep  |          P: a parent,  P*: actual parent,  N: neighbor |")

def console_middle(showHelp):
  print("|----------------------------------------------------------------------------------------------------------------------------------------------|")
  if showHelp:
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
      print("|     h+enter    : Show available commands                                                                                                     |")
  print("|----------------------------------------------------------------------------------------------------------------------------------------------|")
  print("|--------------------------------------------------|               CONSOLE                  |--------------------------------------------------|")
  print("|----------------------------------------------------------------------------------------------------------------------------------------------|")

def log_contact_data(seq):
    ''' this function dump the contact data in the contact file, the only use of contact file is
        further processing'''
    timestamp = seq.timestamp
    source = seq.nodeid
    logstring = "{},{},".format(timestamp, source)
    # g.net.addPrint("[GGG] len datalist {}".format(len(seq.datalist)))
    for ct in seq.datalist:
        logstring += "{:012X},{},{},{},".format(ct.nodeid,ct.lastRSSI, ct.maxRSSI,ct.pktCounter)
    g.contactLogger.info(logstring)

def build_outgoing_serial_message(pkttype, ser_payload):
    ''' it creates the serial message to be sent to the texas board, that will be there converted
        in a message for the network'''

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
      
    #add the checksum
    packet=packet+cs.to_bytes(1, byteorder='big', signed=False)

    #net.addPrint("[DEBUG] Packet : "+str(packet))
    ascii_packet=''.join('%02X'%i for i in packet)

    return ascii_packet.encode('utf-8')

def send_serial_msg(message):

    line = message + par.SER_END_CHAR
    
    if par.TANSMIT_ONE_CHAR_AT_THE_TIME:
        for c in line: #this makes it deadly slowly (1 byte every 2ms more or less). However during OTA this makes the transfer safer if small buffer on the sink is available
            g.ser.write(bytes([c]))
    else:
        g.ser.write(line)

    g.ser.flush()
    g.UARTLogger.info('[GATEWAY] ' + str(line))