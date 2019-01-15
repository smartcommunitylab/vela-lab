Gateway
========================

The Gateway is implemented in python 3 on a Raspberry Pi 3 running Raspiban (ver. June 2017, Kernal 4.9).

Its main functions are:
- read incoming data from the Mesh Sink via serial port (virtual serial port on USB, /dev/ttyACM0)
- defragment (optionally) and decode data from the sink and recode it to the desired internal structures and into a json object
- send the data to the smartcommunity back-end server and log the data
- use the Mesh Sink to send configuration messages into the mesh network based on user input
- log events and data for debugging and testing purposes

