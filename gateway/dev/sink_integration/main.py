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
SERIAL_PORT = "/dev/ttyUSB0"

endchar = 0x0a
SER_END_CHAR = endchar.to_bytes(1, byteorder='big', signed=False)

SINGLE_REPORT_SIZE = 9
MAX_ONGOING_SEQUENCES = 10
MAX_PACKET_PAYLOAD = 54

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

nameContactLog = "contactLogger"
filenameContactLog = timestr + "-contact.log"
contactlog_handler = logging.FileHandler(filenameContactLog)
contact_formatter = logging.Formatter('%(message)s')
contactlog_handler.setFormatter(contact_formatter)
contactLogger = logging.getLogger(nameContactLog)
contactLogger.setLevel(LOG_LEVEL)
contactLogger.addHandler(contactlog_handler)



@unique
class PacketType(IntEnum):
    network_new_sequence =          0x0100
    network_active_sequence =       0x0101
    network_last_sequence =         0x0102
    network_bat_data =              0x0200
    network_request_ping =          0xF000
    network_respond_ping =          0xF001
    network_keep_alive =            0xF010
    ti_set_keep_alive =             0xF801
    nordic_turn_bt_off =            0xF020
    nordic_turn_bt_on =             0xF021
    nordic_turn_bt_on_w_params =    0xF022
    nordic_turn_bt_on_low =         0xF023  #deprecated
    nordic_turn_bt_on_def =         0xF024
    nordic_turn_bt_on_high =        0xF025  #deprecated
    ti_set_batt_info_int =          0xF026
    nordic_reset =                  0xF027

def handle_user_input():
    while 1:
        try:
            user_input = int(input( "Select a configuration\n"
                                    "1 = request ping\n"
                                    "2 = enable bluetooth\n"
                                    "3 = disable bluetooth\n"
                                    "4 = bt_def\n"
                                    "5 = bt_with_params\n"
                                    "6 = enable battery info\n"
                                    "7 = disable battery info\n"
                                    "8 = reset nordic\n"
                                    ">8 = set keep alive interval in seconds\n"))
            if user_input == 1:
                payload = 233
                send_serial_msg(PacketType.network_request_ping, payload.to_bytes(1, byteorder="big", signed=False))
                print("Sent ping request")
                appLogger.debug("[SENDING] Requesting ping")
            elif user_input == 2:
                send_serial_msg(PacketType.nordic_turn_bt_on, None)
                print("Turning bt on")
                appLogger.debug("[SENDING] Enable Bluetooth")
            elif user_input == 3:
                send_serial_msg(PacketType.nordic_turn_bt_off, None)
                print("Turning bt off")
                appLogger.debug("[SENDING] Disable Bluetooth")
            #elif user_input == 4:
            #    send_serial_msg(PacketType.nordic_turn_bt_on_low, None)
            #    print("Turning bt on low")
            #    appLogger.debug("[SENDING] Enable Bluetooth LOW")
            elif user_input == 4:
                send_serial_msg(PacketType.nordic_turn_bt_on_def, None)
                print("Turning bt on def")
                appLogger.debug("[SENDING] Enable Bluetooth DEF")
            #elif user_input == 6:
            #    send_serial_msg(PacketType.nordic_turn_bt_on_high, None)
            #    print("Turning bt on high")
            #    appLogger.debug("[SENDING] Enable Bluetooth HIGH")
            elif user_input == 5:
                print("Turn on bt with params")
                appLogger.debug("[SENDING] Enable Bluetooth with params")
                SCAN_INTERVAL_MS = 10000
                SCAN_WINDOW_MS = 2500
                SCAN_TIMEOUT_S = 0
                REPORT_TIMEOUT_S = 20

                active_scan = 1
                scan_interval = int(SCAN_INTERVAL_MS*1000/625)
                scan_window = int(SCAN_WINDOW_MS*1000/625)
                timeout = int(SCAN_TIMEOUT_S)
                report_timeout_ms = int(REPORT_TIMEOUT_S*1000)
                bactive_scan = active_scan.to_bytes(1, byteorder="big", signed=False)
                bscan_interval = scan_interval.to_bytes(2, byteorder="big", signed=False)
                bscan_window = scan_window.to_bytes(2, byteorder="big", signed=False)
                btimeout = timeout.to_bytes(2, byteorder="big", signed=False)
                breport_timeout_ms = report_timeout_ms.to_bytes(4, byteorder="big", signed=False)
                payload = bactive_scan + bscan_interval + bscan_window + btimeout + breport_timeout_ms
                send_serial_msg(PacketType.nordic_turn_bt_on_w_params, payload)
            elif user_input == 6:
                bat_info_interval_s = 60
                send_serial_msg(PacketType.ti_set_batt_info_int, bat_info_interval_s.to_bytes(1, byteorder="big", signed=False))
                print("Enable battery info with interval: "+str(bat_info_interval_s))
            elif user_input == 7:
                bat_info_interval_s = 0
                send_serial_msg(PacketType.ti_set_batt_info_int, bat_info_interval_s.to_bytes(1, byteorder="big", signed=False))
                print("Disabling battery info")
            elif user_input == 8:
                send_serial_msg(PacketType.nordic_reset, None)
                print("Reset bluetooth")
            elif user_input > 8:
                interval = user_input
                send_serial_msg(PacketType.ti_set_keep_alive, interval.to_bytes(1, byteorder="big", signed=False))
                print("Setting keep alive")
        except ValueError:
            print("read failed")


def send_serial_msg(pkttype, ser_payload):
    payload_size = 0

    if ser_payload is not None:
        payload_size = len(ser_payload)

    packet = payload_size.to_bytes(length=1, byteorder='big', signed=False) + pkttype.to_bytes(length=2, byteorder='big', signed=False)

    if ser_payload is not None:
        packet=packet+ser_payload

    ascii_packet=''.join('%02X'%i for i in packet)
	
    #print("ascii_packet = " + ascii_packet)

    ser.write(ascii_packet.encode('utf-8'))
    ser.write(SER_END_CHAR)
    dataLogger.info("GATEWAY 0x {0} ".format((packet+SER_END_CHAR).hex()))


def decode_payload(seqid, size, packettype, pktnum):
    raw_data = "Node {0} ".format(messageSequenceList[seqid].nodeid) + "0x {:04X} ".format(packettype) + "0x {:02X}".format(pktnum) + " 0x "
    try:
        for x in range(round(size / SINGLE_REPORT_SIZE)):
            nid = int(ser.read(6).hex(), 16)
            lastrssi = int(ser.read(1).hex(), 16)
            maxrssi = int(ser.read(1).hex(), 16)
            pktcounter = int(ser.read(1).hex(), 16)
            contact = ContactData(nid, lastrssi, maxrssi, pktcounter)
            messageSequenceList[seqid].datalist.append(contact)
            messageSequenceList[seqid].datacounter += SINGLE_REPORT_SIZE
            raw_data += "{:012X}".format(nid, 8) + '{:02X}'.format(lastrssi) + '{:02X}'.format(maxrssi) + '{:02X}'.format(pktcounter)
    except ValueError:
        print("[DecodePayload] Payload size to decode is smaller than available bytes")
        appLogger.warning("[Node {0}] Requested to decode more bytes than available. Requested: {1}"
                          .format(messageSequenceList[seqid].nodeid, size))
    finally:
        dataLogger.info(raw_data)
        return


def log_contact_data(seqid):
    seq = messageSequenceList[seqid]
    timestamp = seq.timestamp
    source = seq.nodeid
    logstring = "{0} Node {1} ".format(timestamp, source)
    for x in range(len(seq.datalist)):
        logstring += "{:012X}".format(seq.datalist[x].nodeid) + '{:02X}'.format(seq.datalist[x].lastRSSI) + '{:02X}'.format(seq.datalist[x].maxRSSI) + '{:02X}'.format(seq.datalist[x].pktCounter)

    contactLogger.warning(logstring)

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
                                        dataLogger.info("Node {0} 0x {1} 0x {2} 0x {3}".format(nodeid, '{:04x}'.format(pkttype), '{:02x}'.format(pktnum),
                                                                                      '{:x}'.format(int(ser.read(MAX_PACKET_PAYLOAD).hex(), 16))))
                                    else:
                                        dataLogger.info("Node {0} 0x {1} 0x {2} 0x {3}".format(nodeid, '{:04x}'.format(pkttype), '{:02x}'.format(pktnum),
                                                                                      '{:x}'.format(int(ser.read(remainingDataSize).hex(), 16))))
                                    continue
                                else:
                                    print("Previous sequence has not been completed yet")
                                    # TODO what to do now? For now, assume last packet was dropped
                                    # TODO send received data instead of deleting it all
                                    appLogger.info("[Node {0}] Received new sequence packet "
                                                   "while old sequence has not been completed".format(nodeid))
                                    log_contact_data(seqid)
                                    del messageSequenceList[seqid]

                            messageSequenceList.append(MessageSequence(datetime.datetime.fromtimestamp(time.time()).strftime('%Y-%m-%d %H:%M:%S'),
                                                                       nodeid, pktnum, 0, 0, [], time.time()))
                            seqid = len(messageSequenceList) - 1
                            messageSequenceList[seqid].sequenceSize += datalen

                            if messageSequenceList[seqid].sequenceSize > MAX_PACKET_PAYLOAD - SINGLE_REPORT_SIZE:
                                decode_payload(seqid, MAX_PACKET_PAYLOAD - SINGLE_REPORT_SIZE, PacketType.network_new_sequence, pktnum)

                            else:
                                decode_payload(seqid, datalen, PacketType.network_new_sequence, pktnum)

                                # TODO Only 1 packet in this sequence, so upload this packet already
                                print("Single packet sequence completed")
                                appLogger.debug("[Node {0}] Single packet sequence complete".format(nodeid))
                                log_contact_data(seqid)
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
                            decode_payload(seqid, MAX_PACKET_PAYLOAD, PacketType.network_active_sequence, pktnum)

                        elif PacketType.network_last_sequence == pkttype:
                            # TODO upload data before deleting element from list
                            print("Full message defragmented")
                            seqid = get_sequence_index(nodeid)
                            if seqid == -1:
                                messageSequenceList.append(MessageSequence(datetime.datetime.fromtimestamp(time.time()).strftime('%Y-%m-%d %H:%M:%S'), nodeid,
                                                                           pktnum, 0, 0, [], time.time()))
                                seqid = len(messageSequenceList) - 1
                                decode_payload(seqid, MAX_PACKET_PAYLOAD, PacketType.network_last_sequence, pktnum)
                                print("Message defragmented but header files were never received")
                                print("Nodeid: ", str(messageSequenceList[seqid].nodeid), " datacounter: ",
                                      str(messageSequenceList[seqid].datacounter), " ContactData elements: ",
                                      str(len(messageSequenceList[seqid].datalist)))
                                headerDropCounter += 1
                                appLogger.info("[Node {0}] Message defragmented but header files were never received"
                                                " -  datacounter: {1} ContactData elements: {2}"
                                               .format(nodeid, messageSequenceList[seqid].datacounter,
                                                        len(messageSequenceList[seqid].datalist)))
                                log_contact_data(seqid)
                                del messageSequenceList[seqid]

                            elif messageSequenceList[seqid].sequenceSize == -1:
                                decode_payload(seqid, MAX_PACKET_PAYLOAD, PacketType.network_last_sequence, pktnum)

                                print("Message defragmented but header files were never received")
                                print("Nodeid: ", messageSequenceList[seqid].nodeid, " datacounter: ",
                                      messageSequenceList[seqid].datacounter, " ContactData elements: ",
                                      len(messageSequenceList[seqid].datalist))
                                appLogger.info("[Node {0}] Message defragmented but header files were never received"
                                                " -  datacounter: {1} ContactData elements: "
                                               .format(nodeid, messageSequenceList[seqid].datacounter,
                                                        len(messageSequenceList[seqid].datalist)))
                                log_contact_data(seqid)
                                del messageSequenceList[seqid]

                            else:
                                remainingDataSize = messageSequenceList[seqid].sequenceSize - messageSequenceList[seqid].datacounter
                                if remainingDataSize > MAX_PACKET_PAYLOAD:
                                    decode_payload(seqid, MAX_PACKET_PAYLOAD, PacketType.network_last_sequence, pktnum)
                                else:
                                    decode_payload(seqid, remainingDataSize, PacketType.network_last_sequence, pktnum)
                                if messageSequenceList[seqid].sequenceSize != messageSequenceList[seqid].datacounter:
                                    print("ERROR: Messagesequence ended, but datacounter is not equal to sequencesize")
                                print("Nodeid: ", str(messageSequenceList[seqid].nodeid), " sequencesize: ",
                                      str(messageSequenceList[seqid].sequenceSize), " ContactData elements: ",
                                      str(len(messageSequenceList[seqid].datalist)))
                                appLogger.warning("[Node {0}] Messagesequence ended, but datacounter is not equal to sequencesize -  datacounter: {1}"
                                                   " ContactData elements: {2}".format(nodeid, messageSequenceList[seqid].datacounter,
                                                                                       len(messageSequenceList[seqid].datalist)))
                                log_contact_data(seqid)
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
                            dataLogger.info("Node {0} 0x {1} 0x {2} 0x {3}{4}{5}{6}{7}{8}".format(nodeid, '{:04x}'.format(pkttype), '{:02x}'.format(pktnum), '{:02x}'.format(batCapacity), '{:02x}'.format(int(batSoC * 10)),
                                                                                               '{:02x}'.format(int.from_bytes(bytesELT, byteorder="big", signed=False)), '{:02x}'.format(int(batAvgConsumption * 10)),
                                                                                         '{:02x}'.format(int(batAvgVoltage)), '{:02x}'.format(100 * int(batAvgTemp))))

                        elif PacketType.network_respond_ping == pkttype:
                            payload = int(ser.read(1).hex(), 16)
                            dataLogger.info("Node {0} 0x {1} 0x {2} 0x {3}".format(nodeid, '{:04x}'.format(pkttype), '{:02x}'.format(pktnum), '{:x}'.format(payload)))
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
                            dataLogger.info("Node {0} 0x {1} 0x {2} 0x {3}{4}".format(nodeid, '{:02x}'.format(pkttype), '{:x}'.format(pktnum), '{:02x}'.format(cap), '{:02x}'.format(soc)))

                        else:
                            print("Unknown packettype: ", pkttype)
                            appLogger.warning("[Node {0}] Received unknown packettype: {1}".format(nodeid, hex(pkttype)))
                            dataLogger.info("Node {0} 0x {1} 0x {2}".format(nodeid, '{:02x}'.format(pkttype), '{:x}'.format(pktnum)))

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
                    # print("Turning bt off")
                    # appLogger.debug("[SENDING] Disable Bluetooth")
                    ptype = PacketType.nordic_turn_bt_off
                    # btToggleBool = False
                else:
                    print("Turning bt on")
                    appLogger.debug("[SENDING] Enable Bluetooth")
                    ptype = PacketType.nordic_turn_bt_on
                    btToggleBool = True
                # send_serial_msg(ptype, None)
                btPreviousTime = currentTime


        else: # !ser.is_open (serial port is not open)

            print('Serial Port closed! Trying to open port:', ser.port)

            try:
                ser.open()
            except Exception as e:
                print("Serial Port open exception:", e)
                appLogger.debug("Serial Port exception: %s", e)
                time.sleep(5)
                continue

            print("Serial Port open!")
            appLogger.debug("Serial Port open")


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

