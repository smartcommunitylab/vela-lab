#!/bin/bash
# script that pulls files from the unit test directories
# we expect the directory to contain: Makefile vela_node.c vela_sink.c project-conf.h

./cleanCopied.sh #this ensure to generate an error if for any reason one of the file to be copied is not found

# get the uart files
cp ../basic_uart_init/vela_uart.c .
cp ../basic_uart_init/vela_uart.h .

# get the sender files
cp ../basic_sender_test/vela_sender.c .
cp ../basic_sender_test/vela_sender.h .

# get the sink files
cp ../basic_sender_test/sink_receiver.c .
cp ../basic_sender_test/sink_receiver.h .

