#!/bin/bash
# this build script builds the firmware discarding OTA. It can be usefull for debugging and if OTA have problems
# to clean run ./clean.sh
# This script expects the BOARD as fist argument (for example ./build_node_no_ota.sh launchpad_vela/cc1350)

BOARD=$1
if [ -z "$1" ]
then
    echo BOARD is missing as first argument. Use example: ./build_node_no_ota.sh launchpad_vela/cc1350
    exit 1
fi

echo "Building for: $BOARD" 

export CONTIKI_ROOT=../contiki-ng
export BOARD=$BOARD
export TARGET=cc26x0-cc13x0
export CC26XX_UART_CONF_BAUD_RATE=1000000
export SERIAL_LINE_CONF_BUFSIZE=128
export UIP_CONF_BUFFER_SIZE=256

MY_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"  #give you the full directory name of the script no matter where it is being called from.
cd $MY_DIR  #make sure we are in the proper directory

length=$(($#))
pass_through_agrs=${@:2:$length}

#compile the firmware
make vela_node V=1 OTA=0 NODE=1 CONTIKI_PROJECT=vela_node $pass_through_agrs
cp build/${TARGET}/${BOARD}/vela_node.bin vela_node.bin

rm *.cc26x0-cc13x0 #remove some unused files

