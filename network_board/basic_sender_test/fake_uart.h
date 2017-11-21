#ifndef FAKE_UART_H
#define FAKE_UART_H

PROCESS_NAME (vela_sender_process);
PROCESS_NAME (fake_uart_process);


#define MAX_NUMBER_OF_BT_BEACONS 100
#define SINGLE_NODE_REPORT_SIZE 12
#define MAX_MESH_PAYLOAD_SIZE	MAX_NUMBER_OF_BT_BEACONS*SINGLE_NODE_REPORT_SIZE


#define MAX_RPL_SEND_SIZE 120 

typedef struct
{
    uint8_t  * p_data;      /**< Pointer to data. */
    uint16_t   data_len;    /**< Length of data. */
} data_t;


// the event to be raised between the uart and the sender
process_event_t event_data_ready;

// to initialize the fake_uart
void fake_uart_init();



#endif 
