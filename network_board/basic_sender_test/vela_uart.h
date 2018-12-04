#ifndef VELA_UART_H
#define VELA_UART_H

//PROCESS_NAME (vela_sender_process);
PROCESS_NAME (cc2650_uart_process);


// the event to be raised between the uart and the sender
process_event_t event_data_ready;
process_event_t event_ping_requested;
process_event_t event_pong_received;
process_event_t event_nordic_message_received;

// to initialize the fake_uart
void vela_uart_init();



#endif 
