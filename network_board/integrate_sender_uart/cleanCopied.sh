#!/bin/bash
# script that delete files copied using copyForBuild.sh

# remove the uart files
rm vela_uart.c
rm vela_uart.h

# get the sender files
rm vela_sender.c
rm vela_sender.h

# get the sink files
rm sink_receiver.c
rm sink_receiver.h

