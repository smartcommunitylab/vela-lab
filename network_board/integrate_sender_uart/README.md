Integrate sender uart
========================

This code merges the two projects: basic_uart_init and basic_sender_test to make the firmware complete and able to use both the uart and the wireless mesh network.
The whole project contains just two c files: vela_node.c and vela_sink.c, the other needed files are copied from the other two contiki projects (basic_uart_init, basic_sender_test). To do so, just run the copyForBuild.sh script before building.
