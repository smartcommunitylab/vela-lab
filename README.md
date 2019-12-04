##Smart City Vela Lab
========================

This repository contains the code for the network devices used in the Smart City Lab located in Vela (Trento, Italy).

For cloning:
```bash
git clone https://github.com/smartcommunitylab/vela-lab.git
cd vela-lab
git ckeckout ota
git submodule init
git submodule update
```
Some detail about compilation can be found in README-BUILDING.md


The network components are:
- BLE Beacons: Eddystone or CLIMB (https://www.smartcommunitylab.it/climb-en/) proprietary beacons
- Vela Node: composed by:
    - BLE Scanner: Nordic nRF52 DK (nRF52832 SoC) - also referred as Nordic Board
    - Mesh Node: Texas Instruments cc2650/cc1350 launchpad - also referred as TI Board
    - Interface Board (provides mechanical and electrical connection between the BLE Scanner and the Mesh Node, moreover it manages the solar charger and the battery)
- Mesh Sink: Texas Instruments cc2650/cc1350 launchpad or sensortag
- Gateway: Raspberry PI

The purpose of the network is to monitor for the presence of BLE Beacons nearby the Vela Nodes. The list of presence (ble report) is sent to the sink by the means of the multihop network.
For each BLE Beacon detected by a Vela Node an entry is added to the ble report, this entry contains: the eddystone instance ID, the last value of the RSSI, the max value of the RSSI (during the last observation window) and the amount of beacons packets received from that beacon.
The ble report is generated by the BLE Scanner in particular the related code is in (vela-lab/bt_board/ble_app_vela_lab/main.c -> add_node_to_report_payload(...)).

The BLE Scanner and Mesh Node communicate through an interface based on UART periphearl. A specific communication protocol have been developed, the protocol implementation is in vela-lab/common/lib/uart_util.
The same implementation of this protocol (uart_util) is used both in the BLE Scanner and in the Mesh Node, the adaption of the interface with the proprietary stacks (contiki and nRF SDK) is made through macros (#ifdef CONTIKI #else #endif).

The packet format used by uart_util is briefly documented in vela-lab/doc/raw/uart_packet_struct.xlsx. The most important thing here is that the packet (excluding START_CHAR and END_OF_PACKET) is coded as hexadecimal string.
This means that every raw byte to transfer between the nordic and the TI is converted into two ascii characters (two bytes). It is equivalent to the use of sprintf(coded_byte,"%02x",raw_byte). In this way, the packets over the uart will contain only characters '0' (ascii 0x30) to '9' (ascii 0x39) and 'A' (ascii 0x41)  to 'F' (ascii 0x46).
The packet coding with hex string permits to safetly use START_CHAR=STX (ascii 0x02) and END_OF_PACKET='\n' (ascii 0x0A). Before the adoption of this coding, the packet containing a raw value 10 = 0x0A at any position was a problem since 0x0A is '\n' and it was processed as an END_OF_PACKET.
With hex coding a raw 0x0A (1 byte) is converted into two chars, '0' and 'A' which, in ascii are represented by two bytes: {0x30, 0x39}.

By employing the packet structure in uart_packet_struct.xlsx, debug printf output can be safetly merged with uart_util communication on the same UART interface without any (apparent) issues.
Another benefit of this protocol is that the communication can be sniffed and monitored with a standard serial terminal. Typically, when raw bytes are transmitted over a uart, the serial terminals (putty/teraterm) show only garbage, while using hex string any serial monitor can show the frames perfectly.

The types of packet and their payload description are documented in vela-lab/doc/raw/uart_cmd_def_velaLab.xlsx, these are the only commands that are supported in the communication between the TI and the Nordic over the UART interface. 
Even if there are similarities, the UART commands are not the same as the network commands. Network commands are issued by the Mesh Sink to the Mesh Nodes and they are documented in vela-lab/doc/raw/network_commands.xlsx while the actual definition of codes is in /common/inc/network_messages.h

The Mesh Sink is the root of the WSN and it is its the point where data comes out (for instance the bluetooth reports) or goes in (for instance network commands such as setting the keepalive interval).
Regardless the direction (WSN to gateway, or gateway to WSN) for most of the packet types, the sink just passes them, for some (for instance the ota related packets) some procedures, such as the spitting into smaller chunks, are executed.

Finally, the whole system is managed by the Gateway (which can be a standard pc as well as a single board computer such as a Raspberry) connected to the Mesh Sink running a python script (developed for python3).
On the raspberry, the embedded uart (/dev/ttyS0), exposed on the expansion header, can be used directly on the sensortag (given a proper connection of the TX/RX/Power pins).
Alternatively a launchpad can be used since it embeds a uart to usb bridge (typically /dev/ttyACM0), in that case no hardware related operation is required.

Again, the communication protocol over the UART between the Sink and the Gateway exploits hexadecimal string coding. 
The packet structure is as follow (it is not elsewhere documented): `<IPV6_ADDR_16_BYTES*><PAYLOAD_SIZE_1_BYTE><PACKET_TYPE_2_BYTES**><PAYLOAD_N_BYTES***><PAYLOAD_CHECKSUM_1_BYTE><'\n'>`

\* for broadcast packets (most of commands) set it to 0xFFFFFFFFFFFFFFFFFFFF, otherwise the ipv6 address of the destination node (the Sink cannot be addressed)'

\** these are the same as in vela-lab/doc/raw/network_commands.xlsx

\*** the maximum size of this is given by the uart buffer size defined by the SERIAL_LINE_CONF_BUFSIZE, for the Sink this is defined in in vela-lab/network_board/integrate_sender_uart/build_sink.sh.

Since the data is coded with hex string (2 bytes are allocated for each raw byte), the maximum size for the uart packet (including all the fields) is:  SERIAL_LINE_CONF_BUFSIZE/2.


The python script (vela-lab/gateway/dev/sink_integration/main.py) reads the incoming uart data and decode the packets. A basic Command Line interface is implemented, where basic informations about the WSN is displayed on top, while a console shows log messages.
The python script is lauched by navigating to vela-lab/gateway/dev/sink_integration/ and executing "python3 main.py /dev/ttyS0 57600", where /dev/ttyS0 can be changed with the proper port, and 57600 can be changed if the baudrate of the uart is changed on the Sink. However a simpler way is just to launch startup.sh in the repository root. The idea is to permanently store the default launch parameters (serial interface name, baudrate) into that script.
Once the script is launched, as soon as the data start to arrive from the WSN, the terminal interface becomes populated, by pressing h+enter the list of allowed commands is displayed.
Some command have parameters to be set (for instance the BLE scan parameters can be set), however for now the Command Line Interface does not allow to input the parameters that are hardcoded into vela-lab/gateway/dev/sink_integration/main.py (inside USER_INPUT_THREAD)

The collected data is dumped on log files located in vela-lab/gateway/dev/sink_integration/log, these files are then used to process the data.
Data processing analyzes the collected RSSI values between a BLE Beacon and the BLE Scanner, and it aims at detecting and describing proximity events. A proximity event is defined by the sequence of three phases: approaching, stady, moving away.
When such a sequence is detected a proximity event is generated. It is a json object that contains: 
- scannerID: also referred as NodeID. It is the ID of Vela Node that collected the RSSI
- beaconID: the Eddystone instance ID of the detected beacon
- rssi: the maximum RSSI registered during this event
- start_timestamp: when the proximity event started
- end_timestamp: when the proximity event ended
- event_timestamp: when the BLE Beacon was closest to the BLE Scanner (i.e. the timestamp of the maximum rssi)

The algorith that does this detection is written for Octave (Matlab) and it is launched at a given interval (PROCESS_INTERVAL) by vela-lab/gateway/dev/sink_integration/main.py.
Results are written as json object by the Octave process, then the main.py loads that file and pushes the events to the Thingsboard backend interface.


NOTE:
Special mention is required for the OTA feature. Updating the firmware of Mesh Nodes without the debug probe is allowed through OTA process. The procedures are derived from https://marksolters.com/programming/2016/06/07/contiki-ota.html (no more accessible), sources might be here https://github.com/msolters/ota-server.
To prepare a firmware to be downloaded though OTA a set of operation is required, first the firmware is compiled and properly linked, then the firware metadata* are calculated and appended to the firmware itslef.
To do so a special build script has been prepared (vela-lab/network_board/integrate_sender_uart/build_node_ota.sh, the script contains some comments that should be read before executing it), in order to build the same firmware without the OTA build_node_no_ota.sh can be used.

\*metadata are used to verify the firmware and make the OTA 'safe'

To push the firmware on the nodes:
- launch the python script vela-lab/gateway/dev/sink_integration/main.py
- wait all the nodes to get online
- on the python terminal set the keepalive interval to a long value (just type 250+enter, keepalive will be set to 250s, 255 is the maximum)
- edit vela-lab/network_board/integrate_sender_uart/build_node_ota.sh and verify all the parameters, in particular OTA_VERSION (must be higher than what installed) and OTA_PRE_VERIFIED (must be 0 for OTA, 1 when the firmware is flashed with the debug probe).
- compile the firmware by launching "build_node_ota.sh launchpad_vela/cc1350" where the argument is the Contiki BOARD (can be launchpad_vela/cc2650 in case of 2.4GHz network).
- if the compilation worked there will be a file called 'vela_node_ota' (with no extension) found in vela-lab/network_board/integrate_sender_uart/
- on the python terminal press f+enter, this will start the OTA procedure. It might be long, really long (10-20 min per node)
- once the OTA procedure is over, all the nodes will be rebooted hopefully with the new firmware. If not retry, sometimes it works perfectly, sometimes the CRC check fails on the node and the new firmware gets discarded. 


The folders contained here are used as follow:
- gateway/: contains all the files related to the Gateway. The python script is there, but also the octave algorithm that processes the data to extract proximity events.
- bt_board/: contains the application sources for the BLE Scanner (i.e. the Nordic board).
- network_board/: contains all the Contiki projects related to the Mesh Node and Mesh Sink. The main project is integrate_sender_uart, the others are used for testing/debugging. The folder contains also the full Contiki-NG repository (it is a forked one). Also some external modules are here (the bootloader and ota related libraries)
- common/: contains some files used by more than a component. The uart protocol used for the communication between the Mesh Node and the BLE Scanner is here, as well as some other shared definition.
- hardware/: contains the hardware description (connections bentween the Nordic and the TI) together with the Eagle project of the Interface Board
- doc/: contains some raw,unstructured documentation
- backend/: contains code and files strictly related to the backend server which stores data. Namelly it uses Thingsboard as backend interface. In this folder there are some script to querry the database.

