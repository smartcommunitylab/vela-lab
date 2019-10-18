#ifndef VELA_SENDER_H
#define VELA_SENDER_H

PROCESS_NAME (vela_sender_process);

// the event to be raised between the uart and the sender
process_event_t event_buffer_empty;


// to initialize the fake_uart
void vela_sender_init();

#endif 
