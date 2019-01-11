Mesh devices
========================

This folder contains the code specifically developed for the Mesh Node and for the Mesh Sink. Note that external sources/libraries may be required. Every folder contains all the code required to compile a fully working program. 

The folder basic_i2c_init includes the files for setting up i2c communication to communicate with the fuel gauge on the PCB.

The folder basic_sender_test includes the files used for the network/application layer of the nodes and sink.

The folder basic_sink includes files used for the sink, but these are outdated. The current version is in basic_sender_test.

The folder basic_uart_init contains the files used for the UART communication between the node and the Nordic.

The folder integrate_sender_uart contains the code that will be uploaded to the nodes during deployment. When using the buildscript the following files are copied into this folder:
-basic_uart_init/vela_uart.c 
-basic_uart_init/vela_uart.h 
-basic_sender_test/vela_sender.c 
-basic_sender_test/vela_sender.h 
-basic_sender_test/sink_receiver.c
-basic_sender_test/sink_receiver.h

Warning: Editing these files should be done in their original folder. If you change these files in integrate_sender_uart all progress will be lost when the build script is called. 

