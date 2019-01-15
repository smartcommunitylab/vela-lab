Mesh Sink and Mesh Node: fake uart
========================

This folder contains the code that will run on the Mesh Sink and Mesh Node.
The UART interface in this version is fake and will generate fake data every 60 seconds.
This will only happen on the Mesh Node since the Mesh Sink has a different UART interface just for the gateway.
The generated data will be sent to the vela_sender_process in vela_sender.c and will get fragmented and sent through the mesh network.
Eventually the Mesh Sink will receive this data from the mesh network and it will sent this data to the gateway through UART.
Periodically keep alive messages and battery data will also get sent.
Depending on wether or not there is a valid fuel gauge connected and on the target of the project, fake or real battery data will get sent.

vela_sender.c, vela_sender.h, sink_receiver.c, and sink_receiver.h will also be used by the project integrate_sender_uart.

