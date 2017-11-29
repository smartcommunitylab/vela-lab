
import serial
from array import array
import logging
import datetime
import time

# constantd and defines
# start character = 42 (0x2A)
# start sequence = 4 times start char = 42, 42, 42, 42
START_BUF = 0x2A2A2A
# print("startChar:", START_BUF, "type:", type(START_BUF))

LOG_LEVEL = logging.DEBUG


timestr = time.strftime("%Y%m%d-%H%M%S")
# print(timestr)

filenameLog = timestr + "-application.log"
print(filenameLog)

# Log init
logging.basicConfig(filename=filenameLog,level=LOG_LEVEL,format='%(message)s')
# logging.debug("[" + time.strftime("%H%M%S") + ']' + ' This message should go to the log file')
logging.debug('# Time \tNodeID \tLast \tCounter \tDataLen')


# Serial init
ser = serial.Serial()
ser.port = "/dev/ttyACM0"
#ser.baudrate = 1000000
ser.baudrate = 115200

print('Opening port:', ser.port)
ser.open()
#ser.read(ser.inWaiting())   # read and discard input buffer
ser.reset_input_buffer()

while(1):
    if ser.isOpen():

        bytesWaiting = ser.inWaiting()
        if bytesWaiting > 0:

            # print("Serial Waiting:", bytesWaiting)
            # bufferSerial = ser.read(bytesWaiting)
            # print(bufferSerial)
            # startChar = 1;

            startChar = int.from_bytes(ser.read(1), byteorder='big', signed=False)
            # print("startChar:", startChar, "type:", type(startChar))

            if startChar == 42:

                # print("received 1 Start char. inWaiting:", bytesWaiting)
                startBuf = int.from_bytes(ser.read(3), byteorder='little', signed=False)
                # print("startBuf:", startBuf, "type:", type(startBuf))

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

                        dataBuf = ser.read(dataLen)

                        # print("PKT read! node:", nodeID, " last:", pktLast, " counter:", pktCount, "dataLen:", dataLen, "last bytes:", dataBuf[dataLen-8], dataBuf[dataLen-7], dataBuf[dataLen-6], dataBuf[dataLen-5], dataBuf[dataLen-4], dataBuf[dataLen-3], dataBuf[dataLen-2], dataBuf[dataLen-1])
                        print("PKT read! node:", nodeID, "\tlast:", pktLast, "\tcounter:", pktCount, "\tdataLen:", dataLen, "\tlast bytes:", dataBuf[dataLen-16:])
                        # logging.debug("%s; %s; %s; %s; %s;", time.strftime("%H%M%S"), nodeID, pktLast, pktCount, dataLen)
                        logging.debug("%s\t%s\t%s\t%s\t%s", time.strftime("%H%M%S"), nodeID, pktLast, pktCount, dataLen)

                        lastChar = dataBuf[dataLen-1]
                        if lastChar != 10:
                            ser.reset_input_buffer()


                        # print("\n")
                        # print("Data type:", type(dataBuf))
                        # print(dataBuf)
                        # print("\n")

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
