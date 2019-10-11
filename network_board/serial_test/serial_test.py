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
import array

START_CHAR = '02'
#START_BUF = '2a2a2a'  # this is not really a buffer
BAUD_RATE = 57600 #921600 #1000000
SERIAL_PORT = "/dev/ttyS0" #"/dev/ttyACM0" #"/dev/ttyS0"
endchar = 0x0a
SER_END_CHAR = endchar.to_bytes(1, byteorder='big', signed=False)

if len(sys.argv)>1:
	SERIAL_PORT=sys.argv[1]

if len(sys.argv)>2:
	BAUD_RATE=int(sys.argv[2])


ser = serial.Serial()
ser.port = SERIAL_PORT
ser.baudrate = BAUD_RATE
ser.timeout = 100
TANSMIT_ONE_CHAR_AT_THE_TIME=True   #Setting this to true makes the communication from the gateway to the sink more reliable
#ser.inter_byte_timeout = 0.1 #not the space between stop/start condition

class USER_INPUT_THREAD(threading.Thread):
    def __init__(self, threadID, name):
        threading.Thread.__init__(self)
        self.threadID= threadID
        self.name = name
        self.__loop=True

    def run(self):
        loopNo=0
        x=[]

        TEST_SIZE=511 #max is 511 because the uart buffer size is 512, and the \n has to be appended. Longer packets will be trunkated by the sink

        for d in range(0,TEST_SIZE):
            x.append((d%100)+100)

        payload = array.array('B',x ).tobytes()
        
        while True:
            try:
                send_serial_msg(payload)
                print("Sent message no: "+str(loopNo)+". Message size: "+str(len(payload)))
            except serial.serialutil.SerialException:
                print("Serial port not yet open...")
            
            loopNo=loopNo+1
            time.sleep(1)


def send_serial_msg(message):

    message_b=message+SER_END_CHAR

    send_char_by_char=TANSMIT_ONE_CHAR_AT_THE_TIME
    if send_char_by_char:
        for c in message_b: #this makes it deadly slowly (1 byte every 2ms more or less). However during OTA this makes the transfer safer if small buffer on the sink is available
            ser.write(bytes([c]))
    else:
        ser.write(message_b)
    ser.flush()


user_input_thread=USER_INPUT_THREAD(4,"user input thread")
user_input_thread.setDaemon(True)
user_input_thread.start()

if ser.is_open:
    print("[UART] Serial Port already open! "+ ser.port + " open before initialization... closing first")
    ser.close()
    time.sleep(1)

while 1:
    if ser.is_open:
        try:
            bytesWaiting = ser.in_waiting
        except Exception as e:
            print("[UART] Serial Port input exception:"+ str(e))
            #appLogger.error("Serial Port input exception: {0}".format(e))
            bytesWaiting = 0
            ser.close()
            time.sleep(1)
            continue


        if bytesWaiting > 0:
            rawline = ser.readline() #can block if no timeout is provided when the port is open

            print("Serial data received: "+str(rawline))
        else:
            time.sleep(0.01)
                
    else: # !ser.is_open (serial port is not open)

        print('[UART] Serial Port closed! Trying to open port: %s'% ser.port)

        try:
            ser.open()
            ser.reset_input_buffer()
            ser.reset_output_buffer()
        except Exception as e:
            print("[UART] Serial Port open exception:"+ str(e))
            #appLogger.debug("Serial Port exception: %s", e)
            time.sleep(5)
            continue

        print("[UART] Serial Port open!")
        #appLogger.debug("Serial Port open")

