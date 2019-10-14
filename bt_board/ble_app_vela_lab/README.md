BLE Scanner:ble app vela lab
========================

This main, compiled togheter with the nRF5 SDK (v14.0.0) implements a BLE Scanner that generates BLE Reports containing information about neighboring BLE devices.
In is intended to be connected with the Mesh Node described in another folder of this repository. The communication with this Scanner happen through the UART interface with a protocol defined into vela-lab/common/lib/uart_util.
The only supported toolchain is ARM-GCC, and to ease the setup a buil script is provided (vela-lab/bt_board/ble_app_vela_lab/pca10040/s132/armgcc/build_script.sh).
In order to compile, the NRF52_SDK_ROOT variable of the build_script.sh must be set to the path to the SDK folder.
The code is developed using the SDK v14.0.0, other version such as v14.x should work, version above not.

Read the README-BUILDING.md for few other info on how to build this project.
