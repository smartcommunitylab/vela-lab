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

MAX_CONTACTS_LIST = 1000

# Serial
# start character = 42 (0x2A)
# start sequence = 4 times start char = 42, 42, 42, 42
START_CHAR = 0x2A
START_BUF = 0x2A2A2A
BAUD_RATE = 1000000
SERIAL_PORT = "/dev/ttyACM0"
# SERIAL_PORT = "/dev/ttyUSB0"

# # Structure for contact data
# ContactStruct = namedtuple("ContactStruct", "nodeID lastRSSI maxRSSI pktCount")

# back-end
EVENT_BECON_CONTACT = 901
urlDev_CLIMB = 'https://climbdev.smartcommunitylab.it/v2/api/event/TEST/adca3db3-68d1-4197-b834-a45d61cf1c21/vlab'
urlDev = 'https://climbdev.smartcommunitylab.it/v2/api/event/TEST/4220a8bb-3cf5-4076-b7bd-9e7a1ff7a588/vlab'
urlProd = ' https://climb.smartcommunitylab.it/v2/api/event/TEST/17ee8383-4cb0-4f58-9759-1d76a77f9eff/vlab'
headers = {'Authorization': 'Bearer 831a2cc0-48bd-46ab-ace1-c24f767af8af'}

MIN_POST_PERIOD_S = 60

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


# Data logger
nameDataLog = "dataLogger"
filenameDataLog = timestr + "-data.log"
# formatter = logging.Formatter('%(asctime)s %(levelname)s %(message)s')
formatterDataLog = logging.Formatter('%(message)s')

handler = logging.FileHandler(filenameDataLog)
handler.setFormatter(formatterDataLog)
dataLogger = logging.getLogger(nameDataLog)
dataLogger.setLevel(LOG_LEVEL)
dataLogger.addHandler(handler)

# logging.basicConfig(filename=filenameLog,level=LOG_LEVEL,format='%(message)s')
print("Started data log on file:", filenameDataLog)


# Application logger
nameAppLog = "appLogger"
filenameAppLog = timestr + "-app.log"
# formatter = logging.Formatter('%(asctime)s %(levelname)s %(message)s')
formatterAppLog = logging.Formatter("%(asctime)s %(message)s", "%Y-%m-%d %H:%M:%S")

handler = logging.FileHandler(filenameAppLog)
handler.setFormatter(formatterAppLog)
appLogger = logging.getLogger(nameAppLog)
appLogger.setLevel(LOG_LEVEL)
appLogger.addHandler(handler)

# logging.basicConfig(filename=filenameLog,level=LOG_LEVEL,format='%(message)s')
print("Started application log on file:", filenameAppLog)


# disable log for post requests - generated by urllib3
urllib3_log = logging.getLogger("urllib3")
urllib3_log.setLevel(logging.CRITICAL)



# Serial init
ser = serial.Serial()
ser.port = SERIAL_PORT
ser.baudrate = BAUD_RATE

if ser.is_open:
    print("Serial Port already open!", ser.port, "open before initialization... closing first")
    appLogger.debug("Serial Port already open! %s open before initialization... closing first", ser.port)
    ser.close()
    time.sleep(10)
# end if ser.is_open


# serInit = False
# while serInit == False:
#
#     if ser.is_open:
#         print("Port", ser.port, "already open... closing first")
#         ser.close()
#         time.sleep(10)
#     else:
#         print('Opening port:', ser.port)
#
#         try:
#             ser.open()
#         except Exception as e:
#             print("Serial Port exception!")
#             print(e)
#             time.sleep(10)
#             continue
#
#         print("Serial Port Open!")
#         ser.reset_input_buffer()
#         serInit = True
#     # end if ser.is_open
# # end while serInit




# init lists
tmpContactList = []
contactList = []
timePostLast = time.time()


### run loop ###

while(1):

    if ser.is_open:

        try:
            bytesWaiting = ser.in_waiting
        except Exception as e:
            print("Serial Port input exception:", e)
            appLogger.debug("Serial Port input exception: %s", e)
            bytesWaiting = 0
            ser.close()
            time.sleep(10)
            continue

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

            if startChar == START_CHAR:


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

                    if (dataLen-1) % 9 != 0:
                        print("\n#### Corrupted packetLength #### NodeID:", nodeID, "\tcounter", pktCount, "\tdataLen", dataLen-1, "\tendChar", endChar)
                        appLogger.debug("PKT CorruptedLen NodeID %s counter %d dataLen %d", nodeID, pktCount, dataLen-1)
                        continue
                    #end if

                    # read packet payload
                    dataBuf = ser.read(dataLen-1)
                    endChar = ser.read(2)

                    if endChar != b'\n\x00':
                    # if endChar != b'\n':
                        numBuf = list(dataBuf)
                        payloadStr = ""
                        for i in range(0,dataLen-1):
                            payloadStr = payloadStr + ' {:02X}'.format(numBuf[i])
                        endBuf = list(endChar)
                        payloadStr = payloadStr + " {:02X}".format(endBuf[0]) + " {:02X}".format(endBuf[1])
                        # payloadStr = payloadStr + ' {:02X}'.format(endBuf[0])

                        print("\n#### Corrupted endChar #### NodeID:", nodeID, "\tcounter", pktCount, "\tdataLen", dataLen-1, "\tendChar", endChar)
                        appLogger.debug("PKT CorruptedEnd NodeID %s counter %d dataLen %d payloadHex:%s", nodeID, pktCount, dataLen-1, payloadStr)
                        continue
                    #end if endChar != b'\n\x00'

                    # timestamp for received packet
                    timenow = time.time()
                    timestamp = int(round(timenow * 1000))
                    timestr = time.strftime("%Y-%m-%d %H:%M:%S", time.localtime(timenow))



                    # decode payload: Contact data (9Byte) = [node_id(6Byte)][last_rssi(1Byte)][max_rssi(1Byte)][rx_pkt_count(1Byte)]
                    # node_id saved as big_endiann
                    tmpContactList = []
                    corrupted = False
                    i = 0;
                    while i <= dataLen-10:
                        tmpContact = struct.unpack_from("6sbbB", dataBuf, i)
                        # print("type tmpContact:", type(tmpContact), "tmpContact:", tmpContact)
                        # print("type tmpContact[0]:", type(tmpContact[0]), "tmpContact[0]:", tmpContact[0])

                        beaconIDstr = ""
                        tmplist = list(tmpContact[0])

                        if (tmplist[0] != 0) or (tmplist[1] != 0) or (tmplist[2] != 0) or (tmplist[3] != 0) or (tmplist[4] != 0):
                            corrupted = True
                            break

                        for strid in tmplist:
                            beaconIDstr = beaconIDstr + "{:02X}".format(strid)
                        # print("type nodeIDstr:", type(nodeIDstr), "nodeIDstr:", nodeIDstr)

                        tmpContactList.append({"wsnNodeId":beaconIDstr, "eventType":EVENT_BECON_CONTACT, "timestamp":timestamp, "payload":{"EndNodeID":str(nodeID), "lastRSSI":tmpContact[1], "maxRSSI":tmpContact[2], "pktCounter":tmpContact[3]}})
                        i = i + 9
                    # end while

                    if corrupted:
                        numBuf = list(dataBuf)
                        payloadStr = ""
                        for i in range(0,dataLen-1):
                            payloadStr = payloadStr + ' {:02X}'.format(numBuf[i])
                        endBuf = list(endChar)
                        # payloadStr = payloadStr + ' {:02X}'.format(endBuf[0]) + ' {:02X}'.format(endBuf[1])
                        payloadStr = payloadStr + ' {:02X}'.format(endBuf[0])

                        print("\n#### Corrupted payload #### NodeID:", nodeID, "\tcounter", pktCount, "\tdataLen", dataLen-1, "\tendChar", endChar)
                        appLogger.debug("PKT CorruptedPayload NodeID %s counter %d dataLen %d payloadHex:%s", nodeID, pktCount, dataLen-1, payloadStr)
                        tmpContactList = []
                        continue
                    #end if corrupted

                    # print pkt header
                    print("")
                    print(timestr, "nodeID:", nodeID, "\tlast:", pktLast, "\tcounter:", pktCount, "\tdataLen:", dataLen-1, "\tendChar:", endChar)

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

                    ### log on local data file
                    # log from contactList
                    contactStr = ""
                    for item in tmpContactList:
                        tmps = " " + item["wsnNodeId"] + " " + str(item["payload"]["lastRSSI"]) + " " + str(item["payload"]["maxRSSI"]) + " " + str(item["payload"]["pktCounter"])
                        contactStr = contactStr + tmps
                    # end for
                    dataLogger.debug("%s %s NodeID %d Last %d Counter %d DataLen %d Contacts [ID-last-max-cnt]%s", timestr, timestamp, nodeID, pktLast, pktCount, dataLen-1, contactStr)


                    ### send data to server
                    contactList.extend(tmpContactList)
                    numContacts = len(contactList)
                    if numContacts > MAX_CONTACTS_LIST:
                        del contactList[:(numContacts-MAX_CONTACTS_LIST)]
                        numContacts = len(contactList)

                    print("Current packet:", len(tmpContactList), "contacts. Buffer to send:", numContacts, "contacts")

                    # timePost = time.time()
                    # if timePost - timePostLast > MIN_POST_PERIOD_S:
                    #     print("POST request sending", numContacts, "contacts...")
                    #
                    #     exc = 0
                    #     # r = requests.post(urlDev, json=contactList, headers=headers)
                    #     try:
                    #         r = requests.post(urlDev, json=contactList, headers=headers)
                    #     except Exception as e:
                    #         print("POST request exception:", e)
                    #         appLogger.debug("POST request exception: %s", e)
                    #         exc = 1
                    #
                    #     if exc == 0:
                    #         if r.status_code == requests.codes.ok:
                    #             print("POST Response: OK")
                    #             appLogger.debug("POST request with %d contacts. Response: OK", numContacts)
                    #             contactList = []
                    #         else:
                    #             print("POST Response: ERROR code:", r.status_code, "error:", r.text)
                    #             appLogger.debug("POST request with %d contacts. Response: ERROR! code: %d error: %s", numContacts, r.status_code, r.text)
                    #         # end if r.status_code
                    #     # end if exc == 0
                    #
                    #     timePostLast = time.time()

                    # end if timePost - timePostLast > MIN_POST_PERIOD_S


                    ### cleanup
                    dataBuf = None

                # end if startBuf == START_BUF
            else:
                # received random char NOT packet start
                # startList = list(startChar)
                payloadStr = "{:02X}".format(startChar)
                print(payloadStr, end=" ")
                appLogger.debug("START charHex: %s", payloadStr)
            # end if startChar == 42

        # end if bytesWaiting > 0

    else:

        print('Serial Port closed! Trying to open port:', ser.port)

        try:
            ser.open()
        except Exception as e:
            print("Serial Port open exception:", e)
            appLogger.debug("Serial Port exception: %s", e)
            time.sleep(10)
            continue

        print("Serial Port open!")
        appLogger.debug("Serial Port open")
        ser.reset_input_buffer()
        contactList = []


    # end if ser.is_open

# end while(1)
