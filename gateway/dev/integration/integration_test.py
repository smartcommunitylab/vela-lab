# integration between serial and http
# contact array as events (best option)
# [
# {
#     "wsnNodeId" : <BeaconID> "string"
#     "eventType" : 901,
#     "timestamp" : <timestamp>,
#     "payload" : {
# 		"EndNodeID"   : <nodeID> "string"
# 		"lastRSSI"   : <int>
# 		"maxRSSI"  	 : <int>
# 		"pktCounter" : <int>
#     }
# }


import serial
import logging
import datetime
import time
import struct
import requests
import json


from collections import namedtuple
from array import array


### global data and defines ###
timeStart = int(time.time())

MAX_CONTACTS_LIST = 10

# Serial
# start character = 42 (0x2A)
# start sequence = 4 times start char = 42, 42, 42, 42
START_BUF = 0x2A2A2A
BAUD_RATE = 1000000;
SERIAL_PORT = "/dev/ttyACM0"
# SERIAL_PORT = "/dev/ttyUSB0"

# # Structure for contact data
# ContactStruct = namedtuple("ContactStruct", "nodeID lastRSSI maxRSSI pktCount")

# back-end
EVENT_BECON_CONTACT = 901;
urlDev_CLIMB = 'https://climbdev.smartcommunitylab.it/v2/api/event/TEST/adca3db3-68d1-4197-b834-a45d61cf1c21/vlab'
urlDev = 'https://climbdev.smartcommunitylab.it/v2/api/event/TEST/4220a8bb-3cf5-4076-b7bd-9e7a1ff7a588/vlab'
urlProd = ' https://climb.smartcommunitylab.it/v2/api/event/TEST/17ee8383-4cb0-4f58-9759-1d76a77f9eff/vlab'
headers = {'Authorization': 'Bearer 831a2cc0-48bd-46ab-ace1-c24f767af8af'}

MIN_POST_PERIOD_S = 2

# contactSingle = [
# {
#     "wsnNodeId" : "Beaconid_01",
#     "eventType" : EVENT_BECON_CONTACT,
#     "timestamp" : timeStart,
#     "payload" : {
# 		"EndNodeID": "VelaLab_EndNode_05",
# 		"lastRSSI": -30,
# 		"maxRSSI": -20,
# 		"pktCounter" : 15
#     }
# }
# ]
# {"wsnNodeId":"Beaconid_01", "eventType":EVENT_BECON_CONTACT, "timestamp":timeStart, "payload":{"EndNodeID":"VelaLab_EndNode_05", "lastRSSI":-30, "maxRSSI":-20, "pktCounter":15}}


### Init ###

# Log init
LOG_LEVEL = logging.DEBUG
timestr = time.strftime("%Y%m%d-%H%M%S")
filenameLog = timestr + "-applog.log"
logging.basicConfig(filename=filenameLog,level=LOG_LEVEL,format='%(message)s')
print("Started log file:", filenameLog)

# disable log for requests lib
urllib3_log = logging.getLogger("urllib3")
urllib3_log.setLevel(logging.CRITICAL)

# Serial init
ser = serial.Serial()
ser.port = SERIAL_PORT
ser.baudrate = BAUD_RATE

print('Opening port:', ser.port)
ser.open()
#ser.read(ser.inWaiting())   # read and discard input buffer
ser.reset_input_buffer()

tmpContactList = []
contactList = []
timePostLast = time.time()

### run loop ###

while(1):

    if ser.isOpen():

        bytesWaiting = ser.inWaiting()

        if bytesWaiting > 0:


            # to print raw data in hex decomment here
            # print("\nSerial Waiting:", bytesWaiting)
            # bufferSerial = ser.read(bytesWaiting)
            # bufferSerialNum = list(bufferSerial)
            # for i in range(0,bytesWaiting):
            #     print("", format(bufferSerialNum[i], "02X"), end='', flush=True)
            # startChar = 1;

            # to start decoding packets decmment here
            startChar = int.from_bytes(ser.read(1), byteorder='big', signed=False)

            if startChar == 42:


                startBuf = int.from_bytes(ser.read(3), byteorder='little', signed=False)

                if startBuf == START_BUF:
                    # Received START: decode packet header

                    nodeID = int.from_bytes(ser.read(1), byteorder='little', signed=False)

                    counter = int.from_bytes(ser.read(1), byteorder='little', signed=False)
                    pktLast = (counter & 128)
                    pktCount = counter & 127
                    # print("Counter:", pktCount, "type:", type(pktCount))

                    tmpBuf = ser.read(2)
                    dataLen = int.from_bytes(tmpBuf, byteorder='little', signed=False)
                    # print("Data Length:", dataLen, "type:", type(dataLen))

                    # read packet payload
                    dataBuf = ser.read(dataLen-1)
                    endChar = ser.read(2)
                    # print("type:", type(dataBuf), "dataBuf:", dataBuf)

                    # timestamp fpr received packet
                    timenow = time.time()
                    timestamp = int(timenow)
                    times = time.gmtime(timenow)
                    timestr = time.strftime("%Y-%m-%d-%H:%M:%S", times)

                    # decode payload: Contact data (9Byte) = [node_id(6Byte)][last_rssi(1Byte)][max_rssi(1Byte)][rx_pkt_count(1Byte)]
                    # node_id saved as big_endiann
                    tmpContactList = []
                    i = 0;
                    while i < dataLen-1:
                        tmpContact = struct.unpack_from("6sbbB", dataBuf, i)
                        # print("type tmpContact:", type(tmpContact), "tmpContact:", tmpContact)
                        # print("type tmpContact[0]:", type(tmpContact[0]), "tmpContact[0]:", tmpContact[0])

                        beaconIDstr = ""
                        tmplist = list(tmpContact[0])
                        for strid in tmplist:
                            beaconIDstr = beaconIDstr + "{:02X}".format(strid)
                        # print("type nodeIDstr:", type(nodeIDstr), "nodeIDstr:", nodeIDstr)

                        tmpContactList.append({"wsnNodeId":beaconIDstr, "eventType":EVENT_BECON_CONTACT, "timestamp":timestamp*1000, "payload":{"EndNodeID":str(nodeID), "lastRSSI":tmpContact[1], "maxRSSI":tmpContact[2], "pktCounter":tmpContact[3]}})
                        i = i + 9
                    # end while


                    ### print on terminal
                    # print pkt header
                    print("")
                    print(timestr, "nodeID:", nodeID, "\tlast:", pktLast, "\tcounter:", pktCount, "\tdataLen:", dataLen-1)

                    # print packet payload as hex
                    # numBuf = list(dataBuf)
                    # payloadStr = "hex:"
                    # for i in range(0,dataLen-1):
                    # 	print(' {:02X}'.format(numBuf[i]), end='')
                    #     payloadStr = payloadStr + ' {:02X}'.format(numBuf[i])
                    #
                    # print("")

                    # print contacts from contactList
                    for item in tmpContactList:
                        print("BeaconID: ", item["wsnNodeId"], end=" ")
                        # print(" Timestamp: ", item["timestamp"], end="")
                        print("lastRSSI: ", item["payload"]["lastRSSI"], end=" ")
                        print("maxRSSI: ", item["payload"]["maxRSSI"], end=" ")
                        print("pktcounter: ", item["payload"]["pktCounter"])
                    ## end for

                    ### log on local file
                    # log from contactList
                    contactStr = ""
                    for item in tmpContactList:
                        tmps = " " + item["wsnNodeId"] + " " + str(item["payload"]["lastRSSI"]) + " " + str(item["payload"]["maxRSSI"]) + " " + str(item["payload"]["pktCounter"])
                        contactStr = contactStr + tmps
                    # end for
                    logging.debug("%s %s NodeID %d Last %d Counter %d DataLen %d Contacts [ID-last-max-cnt]%s", timestr, timestamp, nodeID, pktLast, pktCount, dataLen-1, contactStr)


                    ### send data to server
                    contactList.extend(tmpContactList)
                    numContacts = len(contactList)
                    if numContacts > MAX_CONTACTS_LIST:
                        del contactList[:(numContacts-MAX_CONTACTS_LIST)]
                        numContacts = len(contactList)

                    print("Current packet:", len(tmpContactList), "contacts. | Buffer to send", numContacts, "contacts")

                    timePost = time.time()
                    if timePost - timePostLast > MIN_POST_PERIOD_S:
                        print("POST request: sending", numContacts, "contacts...")

                        exc = 0
                        # r = requests.post(urlDev, json=contactList, headers=headers)
                        try:
                            r = requests.post(urlDev, json=contactList, headers=headers)
                        except requests.exceptions.RequestException as re:
                            print("Request exception!")
                            print(re)
                            exc = 1
                        except Exception as e:
                            print("Other exception!")
                            print(e)
                            exc = 1

                        if exc == 0:
                            if r.status_code == requests.codes.ok:
                                print("Response: OK")
                                contactList = []
                            else:
                                print("Response ERROR CODE:", r.status_code)
                                print("ERROR: ", r.text)
                            # end if r.status_code
                        # end if exc == 0

                        timePostLast = time.time()


                    ### cleanup
                    dataBuf = None


                # end if startBuf == START_BUF
            # end if startChar == 42
        # end if bytesWaiting > 0

    else:
        print("Port closed!!!")
    # end if ser.isOpen()

# end while(1)
