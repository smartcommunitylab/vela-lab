from itertools import chain, starmap
import global_variables as g
import params as par
import binascii
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

def decode_payload(payload, seqid, size, pktnum): #TODO: packettype can be removed
    cur=0
    try:
        for x in range(round(size / par.SINGLE_NODE_REPORT_SIZE)):
            nid = int(payload[cur:cur+12], 16)
            cur=cur+12
            lastrssi = int(payload[cur:cur+2], 16)
            cur=cur+2
            maxrssi = int(payload[cur:cur+2], 16)
            cur=cur+2
            pktcounter = int(payload[cur:cur+2], 16)
            cur=cur+2
            contact = par.ContactData(nid, lastrssi, maxrssi, pktcounter)
            g.messageSequenceList[seqid].datalist.append(contact)
            g.messageSequenceList[seqid].datacounter += par.SINGLE_NODE_REPORT_SIZE

    except ValueError:
        print("[Node {0}] Requested to decode more bytes than available. Requested: {1}"
                          .format(g.messageSequenceList[seqid].nodeid, size))
    finally:
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