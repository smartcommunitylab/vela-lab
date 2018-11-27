#ifndef SINK_RECEIVER_H
#define SINK_RECEIVER_H

#include "net/ip/uip.h"

#define MAX_NODES 15
#define MAX_SERIAL_PAYLOAD_SIZE 51

struct ActiveNode {
    uint8_t nodeId;
    struct timer timeoutTimer;
    uip_ipaddr_t ipaddr;
};

PROCESS_NAME (sink_receiver_process);

// to initialize the fake_uart
void sink_receiver_init();
#endif 
