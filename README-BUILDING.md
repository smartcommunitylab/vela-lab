Building the Smart City Vela Lab
========================

This guide is for linux machines (tested with Ubuntu 16.04 LTS).
This guide is not intended to be super accurate and dumb-proof, some experience with Contiki, arm-gcc, makefile, linux bash is required.

Building the code for the components of the Vela Lab requires some external modules and some external tools not included in this repository.
Before building install the following prerequisite software :
* Install the GCC ARM Embedded toolchain (tested with version 4.9.3-20150529 but is should work also with newer versions)
* Install Python3 if it is not installed
* Download and decompress Nordic nRF5 SDK (https://developer.nordicsemi.com/) (this must be the version 14.0.0, probably it works for version 14.x)
* Download and install Uniflash to flash the firmware on TI devices

Once done clone the repository with
git clone https://github.com/smartcommunitylab/vela-lab.git
git ckeckout ota
git submodule init
git submodule update


Building the BLE Scanner firmware
========================
cd /bt_board/ble_app_vela_lab/pca10040/s132/armgcc

Open build_script.sh with a file editor and set NRF52_SDK_ROOT with the absolute path of the Nordic nRF5 SDK root.
Run it!

./build_script.sh

connect the board to the pc, and to flash it use:

make flash_softdevice
make flash

otherwise use JLink tools to do the same


Building the Mesh Node with OTA support
========================
cd /network_board/integrate_sender_uart

Open build_node_ota.sh and configure it (take a look at the comments there, in particular on the first section).
Some options such as UART baudrate, buffers size are defined in build_sink.sh, then it is better to check.
Now run it with the BOARD as argument!

./build_node_ota.sh launchpad_vela/cc1350

or, depending on the target board,

./build_node_ota.sh launchpad_vela/cc2650

Use vela_node_with_bootloader.bin and flash it with Uniflash (to download the firmware over the air OTA, see the main README)


Building the Mesh Sink
========================
cd /network_board/integrate_sender_uart

Some options such as UART baudrate, buffers size are defined in build_sink.sh, then it is better to check it 
run the build script with the BOARD as argument

./build_sink.sh launchpad_vela/cc1350

or

./build_sink.sh launchpad_vela/cc2650

Flash vela_sink.bin using Uniflash

