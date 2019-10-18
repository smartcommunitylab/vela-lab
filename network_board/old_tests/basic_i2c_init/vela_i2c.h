#ifndef VELA_UART_H
#define VELA_UART_H

PROCESS_NAME (vela_sender_process);
PROCESS_NAME (cc2650_uart_process);


// the event to be raised between the uart and the sender
process_event_t event_data_ready;

// to initialize the fake_uart
void vela_i2c_init();



#endif 
