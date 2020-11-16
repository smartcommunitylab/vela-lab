""" Parameters for UART and communication protocol over this pheripheral """

BAUD_RATE = 1000000 #57600 #921600 #1000000
TIMEOUT = 100
SERIAL_PORT = "/dev/ttyACM0" #"/dev/ttyACM0" #"/dev/ttyS0"

START_CHAR = '02'
endchar = 0x0a
SER_END_CHAR = endchar.to_bytes(1, byteorder='big', signed=False)

TANSMIT_ONE_CHAR_AT_THE_TIME=True   #Setting this to true makes the communication from the gateway to the sink more reliable. Tested with an ad-hoc firmware. When this is removed the sink receives corrupted data much more frequently
