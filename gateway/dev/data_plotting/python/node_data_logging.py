import logging
import time
import serial

LOG_LEVEL = logging.DEBUG

nameDataLog = "dataLogger"
formatter = logging.Formatter('%(asctime)s %(levelname)s %(message)s', datefmt='%Y/%m/%d %H:%M:%S')
timestr = time.strftime("%Y%m%d-%H%M%S")
filenameDataLog = timestr + "-data.log"
datalog_handler = logging.FileHandler(filenameDataLog)
datalog_handler.setFormatter(formatter)
dataLogger = logging.getLogger(nameDataLog)
dataLogger.setLevel(LOG_LEVEL)
dataLogger.addHandler(datalog_handler)

BAUD_RATE = 1000000
SERIAL_PORT = "/dev/ttyUSB0"

ser = serial.Serial()
ser.port = SERIAL_PORT
ser.baudrate = BAUD_RATE
ser.timeout = 0.2

if ser.is_open:
    print("Serial Port already open!", ser.port, "open before initialization... closing first")
    ser.close()
    time.sleep(1)

try:
    ser.open()
    while 1:
        if ser.is_open:
            try:
                bytesWaiting = ser.in_waiting
            except Exception as e:
                print("Serial Port input exception:", e)
                bytesWaiting = 0
                ser.close()
                time.sleep(1)
                continue

            if bytesWaiting > 0:
                dataLogger.info(ser.read(1000).decode())

except Exception as e:
    print("Exception: ", e)

