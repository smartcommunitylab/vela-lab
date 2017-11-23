Building the Smart City Vela Lab
========================

This guide is for linux machines (tested with Ubuntu 16.04 LTS).

Building the code for the components of the Vela Lab requires some external modules and some external tools not included in this repository.
Before building install the following prerequisite software :
* Install the GCC ARM Embedded toolchain (tested with version 4.9.3-20150529 but is should work also with newer versions)
* Install Python (tested with version 2.7.12 but it should work for version newer than 2.6)
* Download and decompress Nordic nRF5 SDK (https://developer.nordicsemi.com/) (this must be the version 14.0.0, probably it works for version 14.x)
* Download the Contiki sources (https://github.com/contiki-os/contiki)

Once done clone the repository with
git clone https://github.com/smartcommunitylab/vela-lab.git


Building the BLE Scanner firmware
========================
cd /bt_board/ble_app_vela_lab/pca10040/s132/armgcc

Open build_script.sh with a file editor and substitute {absolute_path_to_nordic_sdk} with the absolute path of the Nordic nRF5 SDK root.

Fix permissions:

chmod +x build_script.sh

Run the build_script.sh with

./build_script.sh


Building the Mesh Node Example
========================
cd /network_board/basic_uart_init

Open build_script.sh with a file editor and substitute {absolute_path_to_contiki} with the absolute path of the Contiki sources root

Fix permissions:

chmod +x build_script.sh

Run the build_script.sh with

./build_script.sh



[Next section TODO]
For flashing the devices these further tools are required:
* NRF tools 
* TI xds probe drivers (included in Code Composer)
