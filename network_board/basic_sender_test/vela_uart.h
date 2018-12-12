#ifndef VELA_UART_H
#define VELA_UART_H

//PROCESS_NAME (vela_sender_process);
PROCESS_NAME (cc2650_uart_process);


// the event to be raised between the uart and the sender
process_event_t event_data_ready;
process_event_t event_ping_requested;
process_event_t event_pong_received;
process_event_t event_nordic_message_received;
process_event_t turn_bt_on;
process_event_t turn_bt_off;
process_event_t turn_bt_on_w_params;
process_event_t turn_bt_on_low;
process_event_t turn_bt_on_def;
process_event_t turn_bt_on_high;
// to initialize the fake_uart
void vela_uart_init();



#endif 
