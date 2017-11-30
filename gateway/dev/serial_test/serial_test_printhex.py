
import serial
from array import array

ser = serial.Serial()
ser.port = "/dev/ttyACM0"
ser.baudrate = 1000000

print('Opening port:', ser.port)
ser.open()
ser.read(ser.inWaiting())   # read and discard input buffer

while(1):
    if ser.isOpen():
        # ser.write("hello")
        nBytesIn = ser.inWaiting()
        if nBytesIn>0:
            bufferString = ser.read(nBytesIn)
            print("#bytes to read:", nBytesIn)
            print(bufferString)
            print("bufferString data type:", type(bufferString))
            print("bufferString size:", len(bufferString))
            print("bufferString:", bufferString[0], bufferString[1], bufferString[2], bufferString[3], bufferString[4], bufferString[5])

            byteBufferIn = bytearray(bufferString)
            print("byteBuffer data type:", type(byteBufferIn))
            print("byteBuffer size:", len(byteBufferIn))
            print("byteBuffer:", byteBufferIn[0], byteBufferIn[1], byteBufferIn[2], byteBufferIn[3], byteBufferIn[4], byteBufferIn[5])

            intBuffer = []
            for i in range(1,len(byteBufferIn)-1,2):
                if byteBufferIn[i] <= ord('9'):
                    tmp1 = byteBufferIn[i] - ord('0')
                else:
                    tmp1 = byteBufferIn[i] - ord('A') + 10

                if byteBufferIn[i+1] <= ord('9'):
                    tmp2 = byteBufferIn[i+1] - ord('0')
                else:
                    tmp2 = byteBufferIn[i+1] - ord('A') + 10

                tmp3 = tmp1*16 + tmp2

                intBuffer.append(tmp3)

            print("intBuffer data type:", type(intBuffer))
            print("intBuffer size:", len(intBuffer))
            print("intBuffer:", intBuffer[0], intBuffer[1], intBuffer[2], intBuffer[3], intBuffer[4], intBuffer[5], intBuffer[6], intBuffer[7], intBuffer[8])
    else:
        print("Port closed!!!")
