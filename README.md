Smart City Vela Lab
========================

This repository contains the code for the embedded network devices used in the Smart City Lab located in Vela (Trento - Italy).
The network components are:
- BLE Beacons: Eddystone or CLIMB (https://github.com/smartcommunitylab/sco.climb.driverapp) proprietary beacons
- BLE Scanner: Nordic nRF52 DK (nRF52832 SoC)
- Mesh Node: Texas Instruments cc2650 launchpad
- Mesh Sink: Texas Instruments cc2650 launchpad
- Gateway: Raspberry PI

The BLE Scanner and the Mesh Node are packed into the same case and the communication between them happen through the UART interface. The same apply for the Mesh Sink and the Gateway, in this case the communication take place over an emulated serial port over USB.

The purpose of this network is to collect 'contacts' of BLE Beacons and push them, through the mesh network, to a cloud infrastructure (database).

To this purpose, the BLE Scanner scans for neighboring devices. When requested, the information regarding contacts (for instance the list of IDs with their RSSI) are passed to the Mesh Node, which injects this data into the mesh network. When the data reach the Mesh Sink it forward it to the Gateway that push the data to the cloud.

The deployment is at very early stage, than the code may have bugs and the compilation process may not be straigh forward, nevertheless we try to describe the steps for setting up the environment in the README-BUILDING.md file in this directory.

