import serial
import time
import datetime
import logging
from recordtype import recordtype
from enum import IntEnum, unique
from datetime import timedelta
import _thread
import readchar

START_CHAR = '2a'
START_BUF = '2a2a2a'  # this is not really a buffer
BAUD_RATE = 1000000
SERIAL_PORT = "/dev/ttyACM0"

endchar = 0x0a
SER_END_CHAR = endchar.to_bytes(1, byteorder='big', signed=False)

SINGLE_REPORT_SIZE = 9
MAX_ONGOING_SEQUENCES = 10
MAX_PACKET_PAYLOAD = 45

MAX_BEACONS = 100

ser = serial.Serial()
ser.port = SERIAL_PORT
ser.baudrate = BAUD_RATE
ser.timeout = 1

MessageSequence = recordtype("MessageSequence", "timestamp,nodeid,lastPktnum,sequenceSize,datacounter,"
                                                "datalist,latestTime")
messageSequenceList = []
ContactData = recordtype("ContactData", "nodeid lastRSSI maxRSSI pktCounter")

firstMessageInSeq = False

NodeDropInfo = recordtype("NodeDropInfo", "nodeid, lastpkt")
nodeDropInfoList = []

ActiveNodes = []

deliverCounter = 0
dropCounter = 0
headerDropCounter = 0
defragmentationCounter = 0

# Timeout variables
previousTimeTimeout = 0
currentTime = 0
timeoutInterval = 300
timeoutTime = 60

previousTimePing = 0
pingInterval = 115

btPreviousTime = 0
btToggleInterval = 1800
btToggleBool = True

# Loggers
LOG_LEVEL = logging.DEBUG

formatter = logging.Formatter('%(asctime)s %(levelname)s %(message)s', datefmt='%Y/%m/%d %H:%M:%S')
timestr = time.strftime("%Y%m%d-%H%M%S")


nameAppLog = "appLogger"
filenameAppLog = timestr + "-app.log"
handler = logging.FileHandler(filenameAppLog)
handler.setFormatter(formatter)
appLogger = logging.getLogger(nameAppLog)
appLogger.setLevel(LOG_LEVEL)
appLogger.addHandler(handler)

nameDataLog = "dataLogger"
filenameDataLog = timestr + "-data.log"
datalog_handler = logging.FileHandler(filenameDataLog)
datalog_handler.setFormatter(formatter)
dataLogger = logging.getLogger(nameDataLog)
dataLogger.setLevel(LOG_LEVEL)
dataLogger.addHandler(datalog_handler)


@unique
class PacketType(IntEnum):
    network_new_sequence = 0x0100
    network_active_sequence = 0x0101
    network_last_sequence = 0x0102
    network_bat_data = 0x0200
    network_request_ping = 0xF000
    network_respond_ping = 0xF001
    network_keep_alive = 0xF010
    ti_set_keep_alive = 0xF801
    nordic_turn_bt_off = 0xF020
    nordic_turn_bt_on = 0xF021
    nordic_turn_bt_on_w_params = 0xF022
    nordic_turn_bt_on_low = 0xF023
    nordic_turn_bt_on_def = 0xF024
    nordic_turn_bt_on_high = 0xF025


def handle_user_input():
    while 1:
        try:
            user_input = int(input("Select a configuration\n1 = request ping\n2 = enable bluetooth\n3 = disable bluetooth\n"
                        "4 = bt low\n5 = bt_def\n6 = bt_high\n >6 = set keep alive interval in seconds"))
            if user_input == 1:
                payload = 233
                send_serial_msg(3, PacketType.network_request_ping, payload.to_bytes(1, byteorder="big", signed=False))
                print("Sent ping request")
            elif user_input == 2:
                send_serial_msg(2, PacketType.nordic_turn_bt_on, 0)
                print("Turning bt on")
            elif user_input == 3:
                send_serial_msg(2, PacketType.nordic_turn_bt_off, 0)
                print("Turning bt off")
            elif user_input == 4:
                send_serial_msg(2, PacketType.nordic_turn_bt_on_low, 0)
                print("Turning bt on low")
            elif user_input == 5:
                send_serial_msg(2, PacketType.nordic_turn_bt_on_def, 0)
                print("Turning bt on def")
            elif user_input == 6:
                send_serial_msg(2, PacketType.nordic_turn_bt_on_high, 0)
                print("Turning bt on high")
            elif user_input > 6:
                interval = user_input
                send_serial_msg(3, PacketType.ti_set_keep_alive, interval.to_bytes(1, byteorder="big", signed=False))
                print("Setting keep alive")
        except ValueError:
            print("read failed")


def send_serial_msg(size, pkttype, payload):
    msg = size.to_bytes(length=1, byteorder='big', signed=False)
    msg += pkttype.to_bytes(length=2, byteorder='big', signed=False)
    if size > 2:
        msg += payload
    msg += SER_END_CHAR
    ser.write(msg)



def decode_payload(seqid, size):
    try:
        for x in range(round(size / SINGLE_REPORT_SIZE)):
            nid = int(ser.read(6).hex(), 16)
            lastrssi = int(ser.read(1).hex(), 16)
            maxrssi = int(ser.read(1).hex(), 16)
            pktcounter = int(ser.read(1).hex(), 16)
            contact = ContactData(nid, lastrssi, maxrssi, pktcounter)
            messageSequenceList[seqid].datalist.append(contact)
            messageSequenceList[seqid].datacounter += SINGLE_REPORT_SIZE
    except ValueError:
        print("[DecodePayload] Payload size to decode is smaller than available bytes")
        appLogger.warning("[Node {0}] Requested to decode more bytes than available. Requested: {1}"
                          .format(messageSequenceList[seqid].nodeid, size))
    finally:
        return


def get_sequence_index(nodeid):
    for x in range(len(messageSequenceList)):
        if messageSequenceList[x].nodeid == nodeid:
            return x
    return -1


def check_for_packetdrop(nodeid, pktnum):
    if pktnum == 0:
        return
    for x in range(len(nodeDropInfoList)):
        if nodeDropInfoList[x].nodeid == nodeid:
            global dropCounter
            dropCounter += pktnum - nodeDropInfoList[x].lastpkt - 1
            appLogger.debug("[Node {0}] Dropped {1} packet(s). Latest packet: {2}, new packet: {3}"
                            .format(nodeid, pktnum - nodeDropInfoList[x].lastpkt - 1, nodeDropInfoList[x].lastpkt,
                                     pktnum))
            return
    nodeDropInfoList.append(NodeDropInfo(nodeid, pktnum))


print("Starting...")

_thread.start_new_thread(handle_user_input, ())

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
                appLogger.error("Serial Port input exception: {0}".format(e))
                bytesWaiting = 0
                ser.close()
                time.sleep(1)
                continue

            if bytesWaiting > 0:
                start = ser.read(1).hex()
                if start == START_CHAR:
                    start = ser.read(3).hex()
                    if start == START_BUF:
                        deliverCounter += 1
                        # TODO change the way pktnum's are being tracked
                        nodeid = int.from_bytes(ser.read(1), "little", signed=False)
                        pkttype = int(ser.read(2).hex(), 16)
                        pktnum = int.from_bytes(ser.read(1), "little", signed=False)
                        print("[New message] nodeid ", nodeid, ", pkttype ", hex(pkttype), ", pktnum ", pktnum)
                        appLogger.info("[New message] nodeid {0}, pkttype {1} pktnum {2}".format(nodeid, hex(pkttype), pktnum))
                        check_for_packetdrop(nodeid, pktnum)

                        if PacketType.network_new_sequence == pkttype:
                            datalen = int(ser.read(2).hex(), 16)
                            if datalen % SINGLE_REPORT_SIZE != 0:
                                print("Invalid datalength: ", datalen)
                                appLogger.warning("[Node {0}] invalid sequencelength: {1}".format(nodeid, datalen))
                            seqid = get_sequence_index(nodeid)
                            if seqid != -1:
                                if messageSequenceList[seqid].lastPktnum == pktnum:
                                    print("Duplicate packet from node ", str(nodeid), " with pktnum ",
                                          str(pktnum))
                                    appLogger.info("[Node {0}] duplicate packet, pktnum: {1}".format(nodeid, pktnum))
                                    # remove duplicate packet data from uart buffer
                                    remainingDataSize = messageSequenceList[seqid].sequenceSize - messageSequenceList[seqid].datacounter
                                    if remainingDataSize >= MAX_PACKET_PAYLOAD:
                                        dataLogger.info("[Node {0}] {1}{2}{3}".format(nodeid, hex(pkttype), hex(pktnum),
                                                                                      ser.read(MAX_PACKET_PAYLOAD)))
                                    else:
                                        dataLogger.info("[Node {0}] {1}{2}{3}".format(nodeid, hex(pkttype), hex(pktnum),
                                                                                      ser.read(remainingDataSize)))
                                    continue
                                else:
                                    print("Previous sequence has not been completed yet")
                                    # TODO what to do now? For now, assume last packet was dropped
                                    # TODO send received data instead of deleting it all
                                    appLogger.info("[Node {0}] Received new sequence packet "
                                                   "while old sequence has not been completed".format(nodeid))
                                    del messageSequenceList[seqid]

                            messageSequenceList.append(MessageSequence(datetime.datetime.fromtimestamp(time.time()).strftime('%Y-%m-%d %H:%M:%S'),
                                                                       nodeid, pktnum, 0, 0, [], time.time()))
                            seqid = len(messageSequenceList) - 1
                            messageSequenceList[seqid].sequenceSize += datalen

                            if messageSequenceList[seqid].sequenceSize >= MAX_PACKET_PAYLOAD:
                                decode_payload(seqid, MAX_PACKET_PAYLOAD)
                                dataString = ""
                                i = 6
                                for x in range(5):
                                    dataString += messageSequenceList[seqid].datalist[len(messageSequenceList[seqid].datalist) - i]
                                    i -= 1
                                dataLogger.info("[Node {0}] {1}{2}{3}".format(nodeid, hex(pkttype), hex(pktnum), dataString))
                            else:
                                decode_payload(seqid, datalen)
                                dataString = ""
                                i = datalen / 9 + 1
                                for x in range(datalen / 9):
                                    dataString += messageSequenceList[seqid].datalist[len(messageSequenceList[seqid].datalist) - i]
                                    i -= 1
                                dataLogger.info("[Node {0}] {1}{2}{3}".format(nodeid, hex(pkttype), hex(pktnum), dataString))

                                # TODO Only 1 packet in this sequence, so upload this packet already
                                print("Single packet sequence completed")
                                appLogger.debug("[Node {0}] Single packet sequence complete".format(nodeid))
                                del messageSequenceList[seqid]

                        elif PacketType.network_active_sequence == pkttype:
                            seqid = get_sequence_index(nodeid)
                            if seqid == -1:
                                print("First part of sequence dropped, creating incomplete sequence at index ",
                                      len(messageSequenceList), " from pktnum ", pktnum)
                                messageSequenceList.append(
                                    MessageSequence(datetime.datetime.fromtimestamp(time.time()).
                                                    strftime('%Y-%m-%d %H:%M:%S'), nodeid,
                                                    pktnum, -1, 0, [], time.time()))
                                seqid = len(messageSequenceList) - 1
                                headerDropCounter += 1

                            elif messageSequenceList[seqid].lastPktnum == pktnum:
                                print("Duplicate packet from node ", nodeid, " with pktnum ",
                                      pktnum)
                                # remove duplicate packet data from uart buffer
                                remainingDataSize = messageSequenceList[seqid].sequenceSize - messageSequenceList[seqid].datacounter
                                if remainingDataSize >= MAX_PACKET_PAYLOAD:
                                    ser.read(MAX_PACKET_PAYLOAD)
                                else:
                                    ser.read(remainingDataSize)
                                    continue

                            messageSequenceList[seqid].lastPktnum = pktnum
                            messageSequenceList[seqid].latestTime = time.time()
                            decode_payload(seqid, MAX_PACKET_PAYLOAD)

                            dataString = ""
                            i = 6
                            for x in range(5):
                                dataString += messageSequenceList[seqid].datalist[len(messageSequenceList[seqid].datalist) - i]
                                i -= 1
                            dataLogger.info("[Node {0}] {1}{2}{3}".format(nodeid, hex(pkttype), hex(pktnum), dataString))

                        elif PacketType.network_last_sequence == pkttype:
                            # TODO upload data before deleting element from list
                            print("Full message defragmented")
                            seqid = get_sequence_index(nodeid)
                            if seqid == -1:
                                messageSequenceList.append(MessageSequence(datetime.datetime.fromtimestamp(time.time()).strftime('%Y-%m-%d %H:%M:%S'), nodeid,
                                                                           pktnum, 0, 0, [], time.time()))
                                seqid = len(messageSequenceList) - 1
                                decode_payload(seqid, MAX_PACKET_PAYLOAD)
                                dataString = ""
                                try:
                                    decode_payload(seqid, MAX_PACKET_PAYLOAD)
                                    i = 6
                                    for x in range(5):
                                        dataString += messageSequenceList[seqid].datalist[
                                            len(messageSequenceList[seqid].datalist) - i]
                                        i -= 1
                                except IndexError:
                                    pass
                                finally:
                                    dataLogger.info("[Node {0}] {1}{2}{3}".format(nodeid, hex(pkttype), hex(pktnum), dataString))

                                print("Message defragmented but header files were never received")
                                print("Nodeid: ", str(messageSequenceList[seqid].nodeid), " datacounter: ",
                                      str(messageSequenceList[seqid].datacounter), " ContactData elements: ",
                                      str(len(messageSequenceList[seqid].datalist)))
                                headerDropCounter += 1
                                appLogger.info("[Node {0}] Message defragmented but header files were never received"
                                                " -  datacounter: {1} ContactData elements: {2}"
                                               .format(nodeid, messageSequenceList[seqid].datacounter,
                                                        len(messageSequenceList[seqid].datalist)))
                                del messageSequenceList[seqid]

                            elif messageSequenceList[seqid].sequenceSize == -1:
                                decode_payload(seqid, MAX_PACKET_PAYLOAD)

                                dataString = ""
                                try:
                                    decode_payload(seqid, MAX_PACKET_PAYLOAD)
                                    i = 6
                                    for x in range(5):
                                        dataString += messageSequenceList[seqid].datalist[
                                            len(messageSequenceList[seqid].datalist) - i]
                                        i -= 1
                                except IndexError:
                                    pass
                                finally:
                                    dataLogger.info(
                                        "[Node {0}] {1}{2}{3}".format(nodeid, hex(pkttype), hex(pktnum), dataString))

                                print("Message defragmented but header files were never received")
                                print("Nodeid: ", messageSequenceList[seqid].nodeid, " datacounter: ",
                                      messageSequenceList[seqid].datacounter, " ContactData elements: ",
                                      len(messageSequenceList[seqid].datalist))
                                appLogger.info("[Node {0}] Message defragmented but header files were never received"
                                                " -  datacounter: {1} ContactData elements: "
                                               .format(nodeid, messageSequenceList[seqid].datacounter,
                                                        len(messageSequenceList[seqid].datalist)))
                                del messageSequenceList[seqid]

                            else:
                                remainingDataSize = messageSequenceList[seqid].sequenceSize - messageSequenceList[seqid].datacounter
                                decode_payload(seqid, remainingDataSize)
                                dataString = ""
                                i = remainingDataSize / 9 + 1
                                for x in range(int(remainingDataSize / 9)):
                                    dataString += messageSequenceList[seqid].datalist[
                                        len(messageSequenceList[seqid].datalist) - i]
                                    i -= 1
                                dataLogger.info(
                                    "[Node {0}] {1}{2}{3}".format(nodeid, hex(pkttype), hex(pktnum), dataString))
                                if messageSequenceList[seqid].sequenceSize != messageSequenceList[seqid].datacounter:
                                    print("ERROR: Messagesequence ended, but datacounter is not equal to sequencesize")
                                print("Nodeid: ", str(messageSequenceList[seqid].nodeid), " sequencesize: ",
                                      str(messageSequenceList[seqid].sequenceSize), " ContactData elements: ",
                                      str(len(messageSequenceList[seqid].datalist)))
                                appLogger.warning("[Node {0}] Messagesequence ended, but datacounter is not equal to sequencesize -  datacounter: {1}"
                                                   " ContactData elements: {2}".format(nodeid, messageSequenceList[seqid].datacounter,
                                                                                       len(messageSequenceList[seqid].datalist)))
                                del messageSequenceList[seqid]

                        elif PacketType.network_bat_data == pkttype:
                            batCapacity = int.from_bytes(ser.read(2), byteorder="big", signed=False)
                            batSoC = int.from_bytes(ser.read(2), byteorder="big", signed=False) / 10
                            bytesELT = ser.read(2)
                            batELT = str(timedelta(minutes=int.from_bytes(bytesELT, byteorder="big", signed=False)))[:-3] # Convert minutes to hours and minutes
                            batAvgConsumption = int.from_bytes(ser.read(2), byteorder="big", signed=True) / 10
                            batAvgVoltage = int.from_bytes(ser.read(2), byteorder="big", signed=False)
                            batAvgTemp = int.from_bytes(ser.read(2), byteorder="big", signed=True) / 100

                            print("Received bat data, Capacity: {0} mAh | State Of Charge: {1}% | Estimated Lifetime: {2} (hh:mm) | "
                                  "Average Consumption: {3} mA | Average Battery Voltage: {4} | Average Temperature {5}"
                                  .format(batCapacity, batSoC, batELT, batAvgConsumption, batAvgVoltage, batAvgTemp))
                            appLogger.info("[Node {0}] Received bat data, Capacity: {1} mAh | State Of Charge: {2}% | Estimated Lifetime: {3} (hh:mm) | "
                                            "Average Consumption: {4} mA | Average Battery Voltage: {5} mV | Temperature: {6} *C"
                                           .format(nodeid, batCapacity, batSoC, batELT, batAvgConsumption,
                                                    batAvgVoltage, batAvgTemp))
                            dataLogger.info("[Node {0}] {1}{2}{3}{4}{5}{6}{7}{8}".format(nodeid, pkttype, pktnum, hex(batCapacity), hex(int(batSoC * 10)),
                                                                                            bytesELT, hex(int(batAvgConsumption * 10)),
                                                                                            hex(batAvgVoltage), hex(batAvgTemp)))

                        elif PacketType.network_respond_ping == pkttype:
                            payload = int(ser.read(1).hex(), 16)
                            dataLogger.info("[Node {0}]{1}{2}{3}".format(nodeid, hex(pkttype), hex(pktnum), hex(payload)))
                            if payload == 233:
                                print("Node id ", nodeid, " pinged succesfully!")
                                appLogger.debug("[Node {0}] pinged succesfully!".format(nodeid))
                                if nodeid not in ActiveNodes:
                                    ActiveNodes.append(nodeid)
                            else:
                                print("Wrong ping payload: {0}".format(payload))
                                appLogger.info("[Node {0}] pinged wrong payload: ".format(nodeid, payload))

                        elif PacketType.network_keep_alive == pkttype:
                            cap = int.from_bytes(ser.read(2), byteorder="big", signed=False)
                            soc = int.from_bytes(ser.read(2), byteorder="big", signed=False)
                            print("Received keep alive packet from node ", nodeid, "with cap & soc: ", cap, " ", soc)
                            appLogger.info("[Node {0}] Received keep alive message with capacity: {1} and SoC: {2}".format(nodeid, cap, soc))
                            dataLogger.info("[Node {0}]{1}{2}{3}{4}".format(nodeid, hex(pkttype), hex(pktnum), hex(cap), hex(soc)))

                        else:
                            print("Unknown packettype: ", pkttype)
                            appLogger.warning("[Node {0}] Received unknown packettype: {1}".format(nodeid, hex(pkttype)))
                            dataLogger.info("[Node {0}]{1}{2}".format(nodeid, hex(pkttype), hex(pktnum)))

        currentTime = time.time()
        if currentTime - previousTimeTimeout > timeoutInterval:
            previousTimeTimeout = time.time()
            deletedCounter = 0
            for x in range(len(messageSequenceList)):
                if currentTime - messageSequenceList[x - deletedCounter].latestTime > timeoutTime:
                    # TODO send data before deleting element
                    del messageSequenceList[x - deletedCounter]
                    deletedCounter += 1
                    print("Deleted seqid {0} because of timeout".format(x + deletedCounter))
                    appLogger.debug("Node Sequence timed out")

        if currentTime - btPreviousTime > btToggleInterval:
            ptype = 0
            if btToggleBool:
                print("Turning bt off")
                ptype = PacketType.nordic_turn_bt_off
                btToggleBool = False
            else:
                print("Turning bt on")
                ptype = PacketType.nordic_turn_bt_on
                btToggleBool = True
            send_serial_msg(2, ptype, 0)
            btPreviousTime = currentTime


except UnicodeDecodeError as e:
    pass

except KeyboardInterrupt:
    print("-----Packet delivery stats summary-----")
    print("Total packets delivered: ", deliverCounter)
    print("Total packets dropped: ", dropCounter)
    print("Total header packets dropped: ", headerDropCounter)
    print("Packet delivery rate: ", 100 * (deliverCounter / (deliverCounter + dropCounter)))
    print("Messages defragmented: ", defragmentationCounter)

    appLogger.info("-----Packet delivery stats summary-----")
    appLogger.info("Total packets delivered: {0}".format(deliverCounter))
    appLogger.info("Total packets dropped: {0}".format(dropCounter))
    appLogger.info("Total header packets dropped: {0}".format(headerDropCounter))
    appLogger.info("Packet delivery rate: {0}".format(100 * (deliverCounter / (deliverCounter + dropCounter))))
    appLogger.info("Messages defragmented: {0}".format(defragmentationCounter))
    raise

finally:
    print("done")

