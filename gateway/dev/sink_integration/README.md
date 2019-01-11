The script main.py is the script that will run on the gateway during deployment.

This script has the following functions:
-Receiving packets from the sink through a serial port and interpreting and (optionally) defragmenting the messages.
-Control the nodes in the network based on user input.
-Log all events for debugging purposes
-Log all received contacts from the nodes.
-Log all the received and sent packets (raw).

Bugs:
-The packet delivery stats that are monitored throughout the script and displayed when the script is closed are wrong. The latest packet of a node is not correctly saved in the function check_for_packetdrop().

To be added:
-Uploading all contacts to a server.

Defragmentation
When the first part of a fragment is received it is kept as a "sequence". A sequence is a list that will contain all the fragments that belong to eachother. When the last fragment of a sequence is received the data of all the fragments gets logged and then the sequence gets deleted so that a new one can be started. Sequences are based on the ID of the sender of the fragment. A sequence is ended when a fragment with the packet type network_last_sequence is received.

User input
The user input is a number that gets entered into the terminal.
The following options are available:
-Request ping
-Enable bluetooth
-Disable bluetooth
-Enable Bluetooth with default settings
-Enable Bluetooth with custom parameters
-Enable the sending of battery info
-Disable the sending of battery info
-Reset all Nordics
-Set keep alive interval in seconds (9 - 254 seconds)

Logfiles
[timestamp]-data.log contains all the logged raw packets.
[timestamp]-contact.log contains all the logged contacts.
[timestamp]-app.log contains all the logged events.
