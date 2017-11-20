BLE Scanner:ble app vela lab
========================

This code contains the program running on the Bluetooth Scanner device. Its behaviour is strictly correlated with the Mesh Node which is connected to the BLE Scanner through the UART interface.
At the boot the device is initialized but without any command by the Mesh Node the BLE Scanned doesn't do anything else, is up the the Mesh Node to start the Bluetooth and request the periodic Bluetooth contacts report.
This code is the actual code that is supposed to run on the BLE Scanner of the Vela Lab.
This code requires nRF5 SDK (v14.0.0) installed and linked (see the README-BUILDING.md at the repository root).
