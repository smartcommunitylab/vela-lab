Gateway
========================

The Gateway is implemented in python 3 on a Raspberry Pi 3 running Raspiban (ver. June 2017, Kernal 4.9).

Its main functions are:
- read incoming data from the Mesh_Sink via serial port (virutal serial port on USB, /dev/ttyACM0)
- decode data from the sink and recode it to the desired internal structures and into a json object
- send the data to the smartcommunity back-end server

