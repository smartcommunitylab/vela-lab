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


# Serial
# start character = 42 (0x2A)
# start sequence = 4 times start char = 42, 42, 42, 42
START_BUF = 0x2A2A2A
BAUD_RATE = 1000000;
SERIAL_PORT = "/dev/ttyACM0"

# Structure for contact data
ContactStruct = namedtuple("ContactStruct", "nodeID lastRSSI maxRSSI pktCount")

# back-end
EVENT_BECON_CONTACT = 901;
urlDev_CLIMB = 'https://climbdev.smartcommunitylab.it/v2/api/event/TEST/adca3db3-68d1-4197-b834-a45d61cf1c21/vlab'
urlDev = 'https://climbdev.smartcommunitylab.it/v2/api/event/TEST/4220a8bb-3cf5-4076-b7bd-9e7a1ff7a588/vlab'
urlProd = ' https://climb.smartcommunitylab.it/v2/api/event/TEST/17ee8383-4cb0-4f58-9759-1d76a77f9eff/vlab'
headers = {'Authorization': 'Bearer 831a2cc0-48bd-46ab-ace1-c24f767af8af'}


fakeDataPayload = [
{
    "wsnNodeId" : "Beaconid_01",
    "eventType" : EVENT_BECON_CONTACT,
    "timestamp" : timeStart,
    "payload" : {
		"EndNodeID": "VelaLab_EndNode_05",
		"lastRSSI": -30,
		"maxRSSI": -20,
		"pktCounter" : 15
    }
},
{
    "wsnNodeId" : "Beaconid_01",
    "eventType" : EVENT_BECON_CONTACT,
    "timestamp" : timeStart,
    "payload" : {
		"EndNodeID" : "VelaLab_EndNode_05",
		"lastRSSI" : -31,
		"maxRSSI" : -21,
		"pktCounter" : 16
    }
},
{
    "wsnNodeId" : "Beaconid_01",
    "eventType" : EVENT_BECON_CONTACT,
    "timestamp" : timeStart,
    "payload" : {
		"EndNodeID" : "VelaLab_EndNode_03",
		"lastRSSI" : -32,
		"maxRSSI" : -22,
		"pktCounter" : 17
    }
}
]


### Init ###

# Log init
LOG_LEVEL = logging.DEBUG
timestr = time.strftime("%Y%m%d-%H%M%S")
filenameLog = timestr + "-applog.log"
# logging.basicConfig(filename=filenameLog,level=LOG_LEVEL,format='%(asctime)s %(message)s', datefmt='[%Y-%m-%d - %H:%M:%S]')
logging.basicConfig(filename=filenameLog,level=LOG_LEVEL,format='%(message)s')
print("Started log file:", filenameLog)


# Serial init
ser = serial.Serial()
ser.port = SERIAL_PORT
ser.baudrate = BAUD_RATE

print('Opening port:', ser.port)
ser.open()
#ser.read(ser.inWaiting())   # read and discard input buffer
ser.reset_input_buffer()


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

                    # print("Received START")

                    nodeID = int.from_bytes(ser.read(1), byteorder='little', signed=False)
                    # print("nodeID:", nodeID, "type:", type(nodeID))

                    # counter = ser.read(1)
                    counter = int.from_bytes(ser.read(1), byteorder='little', signed=False)
                    pktLast = (counter & 128)
                    pktCount = counter & 127
                    # print("Counter:", pktCount, "type:", type(pktCount))

                    tmpBuf = ser.read(2)
                    dataLen = int.from_bytes(tmpBuf, byteorder='little', signed=False)
                    # print("Data Length:", dataLen, "type:", type(dataLen))

                    if dataLen < 500:

                        dataBuf = ser.read(dataLen-1)
                        endChar = ser.read(2)
                        # print("type:", type(dataBuf), "dataBuf:", dataBuf)

                        # print("PKT read! node:", nodeID, " last:", pktLast, " counter:", pktCount, "dataLen:", dataLen, "last bytes:", dataBuf[dataLen-8], dataBuf[dataLen-7], dataBuf[dataLen-6], dataBuf[dataLen-5], dataBuf[dataLen-4], dataBuf[dataLen-3], dataBuf[dataLen-2], dataBuf[dataLen-1])
                        # print("PKT read! node:", nodeID, "\tlast:", pktLast, "\tcounter:", pktCount, "\tdataLen:", dataLen, "\tlast bytes:", dataBuf[dataLen-34:])

                        # decode payload: Contact data (9Byte) = [node_id(6Byte)][last_rssi(1Byte)][max_rssi(1Byte)][rx_pkt_count(1Byte)]
                        # node_id saved as big_endiann

                        contactBuffer = []
                        mylist = [];

                        i = 0;
                        while i < dataLen-1:
                            tmpContact = struct.unpack_from("6sbbB", dataBuf, i)

                            # print("type tmpContact:", type(tmpContact), "tmpContact:", tmpContact)
                            # print("type tmpContact[0]:", type(tmpContact[0]), "tmpContact[0]:", tmpContact[0])

                            nodeIDstr = ""
                            tmplist = list(tmpContact[0])
                            for strid in tmplist:
                                nodeIDstr = nodeIDstr + "{:02X}".format(strid)

                            # print("type nodeIDstr:", type(nodeIDstr), "nodeIDstr:", nodeIDstr)

                            # myVal = int.from_bytes(tmpContact[0], byteorder='big', signed=False)
                            # print("type myVal:", type(myVal), "myVal:", myVal)

                            # mylist.append(ContactStruct(int.from_bytes(tmpContact[0], byteorder='big', signed=False), tmpContact[1], tmpContact[2], tmpContact[3]))
                            mylist.append(ContactStruct(nodeIDstr, tmpContact[1], tmpContact[2], tmpContact[3]))

                            i = i + 9


                        # mylist2.extend(struct.iter_unpack("6cbbB", dataBuf))

                        timenow = time.time()
                        timestamp = int(timenow)
                        times = time.gmtime(timenow)
                        timestr = time.strftime("%Y-%m-%d--%H:%M:%S", times)

                        print("\n", timestr, "nodeID:", nodeID, "\tlast:", pktLast, "\tcounter:", pktCount, "\tdataLen:", dataLen-1)
                        # print("type mylist:", type(mylist), "mylist:", mylist)


                        numBuf = list(dataBuf)
                        payloadStr = "hex:"
                        for i in range(0,dataLen-1):
                        	print(' {:02X}'.format(numBuf[i]), end='')
	                        payloadStr = payloadStr + ' {:02X}'.format(numBuf[i])

                        print("")

                        contctsStr = ""
                        for contactItem in mylist:
                            tmps = " " + contactItem.nodeID + " " + str(contactItem.lastRSSI) + " " + str(contactItem.maxRSSI) + " " + str(contactItem.pktCount)
                            contctsStr = contctsStr + tmps

                        # logging.debug("%s %s NodeID %d Last %d Counter %d DataLen %d Payload %s", timestr, timestamp, nodeID, pktLast, pktCount, dataLen-1, payloadStr)
                        logging.debug("%s %s NodeID %d Last %d Counter %d DataLen %d Contacts [ID-last-max-cnt] %s", timestr, timestamp, nodeID, pktLast, pktCount, dataLen-1, contctsStr)

                        dataBuf = None

                    else:
                        bufferSerial = ser.read(ser.inWaiting())
                        print("\nBuffer Serial:")
                        print(bufferSerial)


            # print("#bytes to read:", nBytesIn)
            # print(bufferString)
            # print("bufferString data type:", type(bufferString))
            # print("bufferString size:", len(bufferString))
            # print("bufferString:", bufferString[0], bufferString[1], bufferString[2], bufferString[3], bufferString[4], bufferString[5])
            #
            # byteBufferIn = bytearray(bufferString)
            # print("byteBuffer data type:", type(byteBufferIn))
            # print("byteBuffer size:", len(byteBufferIn))
            # print("byteBuffer:", byteBufferIn[0], byteBufferIn[1], byteBufferIn[2], byteBufferIn[3], byteBufferIn[4], byteBufferIn[5])

            # intBuffer = []
            # for i in range(1,len(byteBufferIn)-1,2):
            #     if byteBufferIn[i] <= ord('9'):
            #         tmp1 = byteBufferIn[i] - ord('0')
            #     else:
            #         tmp1 = byteBufferIn[i] - ord('A') + 10
            #
            #     if byteBufferIn[i+1] <= ord('9'):
            #         tmp2 = byteBufferIn[i+1] - ord('0')
            #     else:
            #         tmp2 = byteBufferIn[i+1] - ord('A') + 10
            #
            #     tmp3 = tmp1*16 + tmp2
            #
            #     intBuffer.append(tmp3)
            #
            # print("intBuffer data type:", type(intBuffer))
            # print("intBuffer size:", len(intBuffer))
            # print("intBuffer:", intBuffer[0], intBuffer[1], intBuffer[2], intBuffer[3], intBuffer[4], intBuffer[5], intBuffer[6], intBuffer[7], intBuffer[8])
    else:
        print("Port closed!!!")
