#ifndef VELA_SENDER_H
#define VELA_SENDER_H

PROCESS_NAME (vela_sender_process);

#define MAX_NUMBER_OF_BT_BEACONS 100
#define SINGLE_NODE_REPORT_SIZE 12
#define MAX_MESH_PAYLOAD_SIZE	MAX_NUMBER_OF_BT_BEACONS*SINGLE_NODE_REPORT_SIZE


#define MAX_RPL_SEND_SIZE 360


// the event to be raised between the uart and the sender
process_event_t event_buffer_empty;


// to initialize the fake_uart
void vela_sender_init();

#endif 
