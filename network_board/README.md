Mesh devices
========================

This folder contains the code specifically developed for the Mesh Node and for the Mesh Sink. Note that external sources/libraries may be required. Every folder contains all the code required to compile a fully working program. 
Contiki project are typically compiled by calling "make". Here (in particular in the integrate_sender_uart) non standard components are included (bootloader, ota), to ease the integration of these components some build script are provided.


- integrate_sender_uart/ is the main one. This is the firmware that is supposed to run on the TI board during a deployment

- contiki-ng/ are the contiki-ng sources. Forked from the official repository.

- basic_sender_test/ includes the files used for the network/application layer of the nodes and sink.

- cooja_basic_sender_test/ the same as the previous, but the target is cooja symulator.

- basic_uart_init/ contains the files used for the UART communication between the node and the Nordic.

- external_modules/ contains sources for the Mesh Sink bootloader and OTA related code.

- old_tests/basic_sink/ includes files used for the sink, but these are outdated. The current version is in basic_sender_test.

- old_tests/basic_i2c_init/ includes the files for setting up i2c communication to communicate with the fuel gauge on the PCB.

- old_tests/serial_test/ contains the firmware and the related python to test the uart communication between the sink and the gateway


