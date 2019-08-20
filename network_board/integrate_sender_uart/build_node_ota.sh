#!/bin/bash
# building an OTA image requires to edit the contiki make structure. I don't know how to do it and this script is a quick and dirty workaround.
# To build the OTA binaries just run this script with no arguments (eventually set the NODEID in command line). It will produce, among the others two files:
# vela_node_ota.bin : its the firmware to be used in OTA updates containing also the metadata (i.e. the image that is sent over the air). It will be flashed at address 0x00002000
# vela_node_with_bootloader.bin : its the master binary to flash on a plain device. It contains the bootloader, metadata and the firmware. You should flash it at address 0x00000000
# to clean you can call "build_sink.sh clean"
# When you prepare an OTA update, remeber to increase the version number when calculating metadata (it is the first hexadecimal argument in the generate-metadata call), otherwise the bootloader won't use the update
# When you prepare an OTA update remember to set OTA_PRE_VERIFIED=0, while if the firmware is flashed with the debugger use OTA_PRE_VERIFIED=1
# Firmware produced with CLEAR_OTA_SLOTS=1 and/or BURN_GOLDEN_IMAGE=1 are not adeguate for a deployment, they are only used to manipulate the external flash, and they should not remain in the internal flash
# to clean run ./clean.sh

# ************************************************ CONFIGURATION ************************************************
CONTIKI_ROOT=/home/giova/workspaces/GIT/contiki-ng
BOARD=launchpad_vela/cc1350

OTA_VERSION=0x0021  #NB: in order to have the firmware recognized as the LATEST and conseguently making the nodes to load it, version number should be higher than the installed one.
OTA_UUID=0xabcd1234
OTA_PRE_VERIFIED=0  #NB: for GOLDEN IMAGE and in general when firmware is going to be loaded through the debugger, set this to 1. For OTA updates set this to 0 - ATTENTION: this not would be true in an ideal world. It appeared that overwriting the OTA metadata onboard requires too much ram that we don't have. For this reason set always set OTA_PRE_VERIFIED=1, the ota will be anyway verified, and if the CRC doesn't match the ota slot will be erased (see verify_ota_slot(...) in ota.c).

CLEAR_OTA_SLOTS=0
BURN_GOLDEN_IMAGE=0
recompile_bootloader=1 #use 1 to recompile the bootloader, otherwise the already compiled one will be used. If you don't set this to 1 (and carefully check the value of CLEAR_OTA_SLOTS and BURN_GOLDEN_IMAGE, in a deployment both should be 0)
# ************************************************ BUILDING ************************************************
export CONTIKI_ROOT=${CONTIKI_ROOT}
export BOARD=${BOARD}
export TARGET=cc26x0-cc13x0
export CC26XX_UART_CONF_BAUD_RATE=1000000
export SERIAL_LINE_CONF_BUFSIZE=128
export UIP_CONF_BUFFER_SIZE=512

./copyForBuild.sh
#compile the firmware
make "$@" vela_node V=0 OTA=1 NODE=1 CONTIKI_PROJECT=vela_node

if [ $recompile_bootloader -eq 1 ]
then
    THIS_FOLDER=$PWD
    cd ./../external_modules/bootloader/
    #compile the bootloader
    make clean
    make bootloader.bin CLEAR_OTA_SLOTS=${CLEAR_OTA_SLOTS} BURN_GOLDEN_IMAGE=${BURN_GOLDEN_IMAGE}
    cd $THIS_FOLDER
fi


#calculate metadata
./../external_modules/generate_metadata/generate-metadata build/${TARGET}/${BOARD}/vela_node.bin ${OTA_VERSION} ${OTA_UUID} ${OTA_PRE_VERIFIED}
cp ./firmware-metadata.bin build/${TARGET}/${BOARD}/firmware-metadata.bin
rm ./firmware-metadata.bin

#merge metadata with the application binary
srec_cat build/${TARGET}/${BOARD}/firmware-metadata.bin -binary build/${TARGET}/${BOARD}/vela_node.bin -binary -offset 0x100 -o build/${TARGET}/${BOARD}/vela_node_ota.bin -binary

#merge projecy (metadata+app binary) with bootloader
srec_cat ./../external_modules/bootloader/bootloader.bin -binary -crop 0x0 0x2000 0x1FFA8 0x20000 build/${TARGET}/${BOARD}/vela_node_ota.bin -binary -offset 0x2000 -crop 0x2000 0x1B000 -o vela_node_with_bootloader.bin -binary

cp build/${TARGET}/${BOARD}/vela_node_ota.bin vela_node_ota.bin
cp build/${TARGET}/${BOARD}/vela_node_ota.bin vela_node_ota #workaround to leave a copy of OTA after clean

rm *.cc26x0-cc13x0 #remove some unused files

