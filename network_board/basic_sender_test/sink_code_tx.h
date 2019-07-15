#ifndef VELA_SINK_CODE_TX_H
#define VELA_SINK_CODE_TX_H

PROCESS_NAME (code_tx_process);


// the event to be raised between the code transmitter and the sender
process_event_t event_code_ready;

// to initialize the fake_uart
void sink_code_tx_init();



#endif 
