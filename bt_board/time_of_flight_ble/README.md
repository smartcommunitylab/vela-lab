Time Of Flight BLE

This project contains sources for getting ToF measure from a BLE connection.
It is based on the Nordic Semiconductor nRF52 platform and it works both on nRF52832 and on nRF52840 MCUs.
The provided example is based on a modified version of ble_app_att_mtu_throughput example provided by Nordic Semiconductor and only the arm GCC toolchain is supported.
The main file of the example is located in /nRF5_SDK_14.0.0_3bcc1f7/examples/ble_central_and_peripheral/experimental/ble_app_att_mtu_throughput/

Before compiling you have to set up the arm GCC toolchain and JLink tools for the Nordic platform.
A step-by-step guide for this is available in the Nordic website: https://devzone.nordicsemi.com/tutorials/b/getting-started/posts/development-with-gcc-and-eclipse

For compile and flash on the PCA10040 (nRF52832):

- cd /nRF5_SDK_14.0.0_3bcc1f7/examples/ble_central_and_peripheral/experimental/ble_app_att_mtu_throughput/pca10040/s132/armgcc

- make

- make flash_softdevice

- make flash


For compile and flash on the PCA10056 (nRF52840):

- cd /nRF5_SDK_14.0.0_3bcc1f7/examples/ble_central_and_peripheral/experimental/ble_app_att_mtu_throughput/pca10056/s140/armgcc

- make

- make flash_softdevice

- make flash


A description of the measurement methodology can be found in the paper "RSSI or Time-of-flight for Bluetooth Low Energy based localization? An experimental evaluation" by Davide Giovanelli and Elisabetta Farella

The code is organized as module (following the programming style of the Nordic Semiconductor SDK) and it can be added to another project by including the files in /nRF5_SDK_14.0.0_3bcc1f7/external/ifs_tof.
For the inclusion of the module please refer to the example main in /nRF5_SDK_14.0.0_3bcc1f7/examples/ble_central_and_peripheral/experimental/ble_app_att_mtu_throughput/main.c

If you use this code for any research project, review article or other document please cite:

Giovanelli, D., & Farella, E. (2018, September). RSSI or Time-of-Flight for Bluetooth Low Energy based localization? An experimental evaluation. In Wireless and Mobile Networking Conference (WMNC), 2018 11th International (pp. 1622-1627). IFIP/IEEE

and/or:

Giovanelli, D., Farella, E., Fontanelli, D., Macii, D. (2018, September). Bluetooth-based Indoor Positioning through ToF and RSSI Data Fusion. In Indoor Positioning and Indoor Navigation Conference (IPIN), 2018 IEEE International (pp. 1-6). IEEE

Our code (the ifs_tof module) is released under Apache License Version 2.0, see LICENSE file in the root of repository.
The rest of the code is released by Nordic Semiconductor and for that refer to Nordic license file in /nRF5_SDK_14.0.0_3bcc1f7/license.txt

DISCLAIMER:
With this repository we are not releasing a product then altough the code has been tested, it is not garantee to work.

