#!/bin/bash
# building an OTA image requires to edit the contiki make structure. I don't know how to do it and this script is a quick and dirty workaround.
# To build the OTA binaries just run this script with the board as argument (for example ./build_node_ota.sh launchpad_vela/cc1350). It will produce, among the others two files:
# vela_node_ota.bin : its the firmware to be used in OTA updates containing also the metadata (i.e. the image that is sent over the air). It will be flashed at address 0x00002000
# vela_node_with_bootloader.bin : its the master binary to flash on a plain device. It contains the bootloader, metadata and the firmware. You should flash it at address 0x00000000
# When you prepare an OTA update, remeber to increase the OTA_VERSION
# When you prepare an OTA update remember to set OTA_PRE_VERIFIED=0, while if the firmware is flashed with the debugger use OTA_PRE_VERIFIED=1
# Firmware produced with CLEAR_OTA_SLOTS=1 and/or BURN_GOLDEN_IMAGE=1 are not adeguate for a deployment, they are only used to manipulate the external flash, and they should not remain in the internal flash
# to clean run ./clean.sh

# ************************************************ CONFIGURATION ************************************************
CONTIKI_ROOT=../contiki-ng
BOARD=$1 #launchpad_vela/cc1350
if [ -z "$1" ]
then
    echo BOARD is missing as first argument. Use example: ./build_node_ota.sh launchpad_vela/cc1350 ota
    exit 1
fi

echo "Building for: $1" 

OTA_VERSION=0x0028  #NB: in order to have the firmware recognized as the LATEST and conseguently making the nodes to install it, version number should be higher than the installed one.
OTA_UUID=0x00000001 #actually never really used, it is just an ID, one can set it to any 32bit value.

FLASH_MECHANISM=$2

if [[ $FLASH_MECHANISM == "external" ]]
then
        echo "FLASH_MECHANISM: $FLASH_MECHANISM"
elif [[ $FLASH_MECHANISM == "ota" ]]
then
        echo "FLASH_MECHANISM: $FLASH_MECHANISM"
else
    FLASH_MECHANISM="ota"   #default value. Change to change the default behaviour
    if [[ $FLASH_MECHANISM == "external" ]]
    then
        echo "The second argument should be \"external\" or \"ota\". Using the default value: external"
    else
        echo "The second argument should be \"external\" or \"ota\". Using the default value: ota"
    fi
fi

if [ $FLASH_MECHANISM == "external" ]; then
    echo "Preparing the firmware to be flashed with external tool (uniflash or gdb debugger)."
    OTA_PRE_VERIFIED=1          #NB: for GOLDEN IMAGE and in general when firmware is going to be loaded through the debugger, set this to 1. For OTA updates set this to 0 - 
elif [ $FLASH_MECHANISM == "ota" ]; then
    echo "Preparing the firmware to be flashed with ota."
    OTA_PRE_VERIFIED=0          #NB: for GOLDEN IMAGE and in general when firmware is going to be loaded through the debugger, set this to 1. For OTA updates set this to 0 - 
fi

CLEAR_OTA_SLOTS=0   #do not use if you don't know what you are doing (this impacts only on the bootloader, on regular deployments this MUST be 0)
BURN_GOLDEN_IMAGE=0 #do not use if you don't know what you are doing (this impacts only on the bootloader, on regular deployments this MUST be 0)
recompile_bootloader=1 #use 1 to recompile the bootloader, otherwise the already compiled one will be used. If you don't know what to do set it to 1.
# ************************************************ BUILDING ************************************************
export CONTIKI_ROOT=${CONTIKI_ROOT}
export BOARD=${BOARD}
export TARGET=cc26x0-cc13x0
export CC26XX_UART_CONF_BAUD_RATE=1000000
export SERIAL_LINE_CONF_BUFSIZE=128
export UIP_CONF_BUFFER_SIZE=800

MY_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"  #give you the full directory name of the script no matter where it is being called from.
cd $MY_DIR  #make sure we are in the proper directory

#compile the firmware
length=$(($#))
pass_through_agrs=${@:3:$length}

make vela_node V=1 OTA=1 NODE=1 CONTIKI_PROJECT=vela_node $pass_through_agrs

if [ $recompile_bootloader -eq 1 ]
then
    CONTIKI_ROOT_ABS=$(python -c "import os.path; print os.path.abspath('${CONTIKI_ROOT}')")

    cd ./../external_modules/bootloader/
    
    #compile the bootloader
    make clean
    make bootloader.bin CLEAR_OTA_SLOTS=${CLEAR_OTA_SLOTS} BURN_GOLDEN_IMAGE=${BURN_GOLDEN_IMAGE} CONTIKI_ROOT=${CONTIKI_ROOT_ABS}
    make bootloader.elf CLEAR_OTA_SLOTS=${CLEAR_OTA_SLOTS} BURN_GOLDEN_IMAGE=${BURN_GOLDEN_IMAGE} CONTIKI_ROOT=${CONTIKI_ROOT_ABS}
    cd $MY_DIR
fi


#calculate metadata
./../external_modules/generate_metadata/generate-metadata build/${TARGET}/${BOARD}/vela_node.bin ${OTA_VERSION} ${OTA_UUID} ${OTA_PRE_VERIFIED}
cp ./firmware-metadata.bin build/${TARGET}/${BOARD}/firmware-metadata.bin
rm ./firmware-metadata.bin

#merge metadata with the application binary
srec_cat build/${TARGET}/${BOARD}/firmware-metadata.bin -binary build/${TARGET}/${BOARD}/vela_node.bin -binary -offset 0x100 -o build/${TARGET}/${BOARD}/vela_node_ota.bin -binary

#merge projecy (metadata+app binary) with bootloader
srec_cat ./../external_modules/bootloader/bootloader.bin -binary -crop 0x0 0x2000 0x1FFA8 0x20000 build/${TARGET}/${BOARD}/vela_node_ota.bin -binary -offset 0x2000 -crop 0x2000 0x1B000 -o build/${TARGET}/${BOARD}/vela_node_with_bootloader.bin -binary

cp build/${TARGET}/${BOARD}/vela_node_ota.bin vela_node_ota.bin
cp build/${TARGET}/${BOARD}/vela_node_ota.bin vela_node_ota #workaround to leave a copy of OTA after clean

rm *.cc26x0-cc13x0 #remove some unused files

