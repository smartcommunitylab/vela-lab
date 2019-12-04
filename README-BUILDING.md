#Building the Smart City Vela Lab
========================

This guide is for linux machines (tested with Ubuntu 16.04 LTS).
This guide is not intended to be super accurate and dumb-proof, some experience with Contiki, arm-gcc, makefile, linux bash is required.

Building the code for the components of the Vela Lab requires some external modules and some external tools not included in this repository.
Before building install the following prerequisite software :
* Install the GCC ARM Embedded toolchain (tested with version 4.9.3-20150529 but is should work also with newer versions)
* Install Python3 if it is not installed
* Download and install Uniflash to flash the firmware on TI devices

Once done clone the repository with
```bash
git clone https://github.com/smartcommunitylab/vela-lab.git
cd vela-lab
git ckeckout ota
git submodule init
git submodule update
```

#Building the BLE Scanner firmware
========================

```bash
cd vela-lab/bt_board/ble_app_vela_lab/pca10040/s132/armgcc
./build_script.sh
```

connect the board to the pc, and to flash it use:

```bash
make flash_softdevice
make flash
```

or

```bash
./build_script.sh flash_softdevice
./build_script.sh flash
```

otherwise use JLink tools to do the same


#Building the Mesh Node with OTA support
========================
If it is the first time, you might need to compile "vela-lab/network_board/external_modules/generate_metadata", so:

```bash
cd vela-lab/network_board/external_modules/generate_metadata
make generate-metadata
```

```bash
cd vela-lab/network_board/integrate_sender_uart
```

Open build_node_ota.sh and configure it (take a look at the comments there, in particular on the first section).
Some options such as UART baudrate, buffers size are defined in build_sink.sh, then it is better to check.
Now run it with the BOARD as argument!

```bash
./build_node_ota.sh launchpad_vela/cc1350
```

or, depending on the target board,

```bash
./build_node_ota.sh launchpad_vela/cc2650
```

Use vela_node_with_bootloader.bin and flash it with Uniflash (to download the firmware over the air OTA, see the main README)


#Building the Mesh Sink
========================
```bash
cd vela-lab/network_board/integrate_sender_uart
```

Some options such as UART baudrate, buffers size are defined in build_sink.sh, then it is better to check it 
run the build script with the BOARD as argument

```bash
./build_sink.sh launchpad_vela/cc1350
```

or

```bash
./build_sink.sh launchpad_vela/cc2650
```

Flash vela_sink.bin using Uniflash

#Using VSCode
========================
All the code in this repo (the firmware for the embedded platforms and the python script that manages the gateway) can be run, flashed and debugged with VSCode IDE. This strongly ease the process since all the command line arguments are already set in tasks.json files.
To do so just load the workspace file common/vscode_workspace/vela_vs_workspace.code-workspace. In order to work, the next extensions should be installed:

- Cortex-Debug
- Python

VSCode will call the commands 'openocd' and 'arm-none-eabi-gdb', in case these two commands are not available in the environment you should add the paths to VSCode configuration files. Take a look at the documentation of the Cortex-Debug extension.
Pay attention because some outdated versions of openocd do not support the launchpad targets, in that case search for an openocd version that supports it (look in the Texas Instruments website)

