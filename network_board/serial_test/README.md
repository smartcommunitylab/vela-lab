Serial test
========================

This code can be used to test the reliability of the uart communication between the Gateway and the Sink. 
In the Sink-to-Gateway direction the communication is reliable, but in the Gateway-to-Sink sometimes the received data is corrupted.
This code (build the firmware by running build.sh and run the python script) just transmits known data {100,101,102,....,100,101,...} and checks for error.
There is a big difference between transmitting a buffer rather than spitting all characters in single transfers. This makes the transfer deadly slow, but no error occours.

