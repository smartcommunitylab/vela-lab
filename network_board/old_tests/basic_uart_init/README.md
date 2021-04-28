Mesh Node: test uart
========================

This code contains an example code of the program running on the Mesh Node (TI cc2650). 
This example do not use any kind of wireless interface.
The aim of this code is to show how to use the UART for communicating with the BLE Scanner. To this purpose the TI send forces an hard reset on the BLE Scanner, waits for the uart_ready packet, and then it start sending initialization packets (set BLE scan parameters, set BLE scan ON, request periodic report).
At this point the reports are automatically sent by the BLE Scanner to the Mesh Node.
As the full report is received it calls report_ready(uint8_t *p_buff, uint16_t size) where 'p_buff' points to the report and 'size' is its size. Note that the data stored in the buffer does not include the TYPE field, it is the raw list of contacts. 
For now the structure of the contacts isn't really fixed, to know the exact format look into: /bt_board/ble_app_vela_lab/main.c : add_node_to_report_payload(...)

