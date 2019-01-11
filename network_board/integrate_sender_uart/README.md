Integrate sender uart
========================

This code merges the two projects: basic_uart_init and basic_sender_test to make the firmware complete and able to use both the uart and the wireless mesh network.
The whole project contains just two c files: vela_node.c and vela_sink.c. The other needed files are copied from:
-basic_uart_init/vela_uart.c 
-basic_uart_init/vela_uart.h 
-basic_sender_test/vela_sender.c 
-basic_sender_test/vela_sender.h 
-basic_sender_test/sink_receiver.c
-basic_sender_test/sink_receiver.h
To do so, just run the copyForBuild.sh script before building.

Warning: Editing these files should be done in their original folder. If you change these files in integrate_sender_uart all progress will be lost when the build script is called.

Within this code are 4 processes.

-Vela_sender_process
This process is responsible for the sending of data through RPL and fragmenting it. The data includes BLE reports, pong messages, and battery data.

-Trickle_protocol_process
This process sends and handles incoming Trickle messages. Trickle messages are used to configure all the nodes in the network.

-Keep_alive_process
This process sends a periodic keep alive message. The interval of these keep alive messages can be changed by the gateway using Trickle messages.

-cc2650_uart_process
This process handles all UART communication between the CC2650 and the Nordic.
