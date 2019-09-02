#!/bin/bash
# this build script builds the firmware discarding OTA. It can be usefull for debugging and if OTA have problems
# to clean run ./clean.sh

export CONTIKI_ROOT=../contiki-ng
export BOARD=launchpad_vela/cc2650
export TARGET=cc26x0-cc13x0
export CC26XX_UART_CONF_BAUD_RATE=1000000
export SERIAL_LINE_CONF_BUFSIZE=128
export UIP_CONF_BUFFER_SIZE=256

#compile the firmware
make "$@" vela_node V=0 OTA=0 NODE=1 CONTIKI_PROJECT=vela_node
cp build/${TARGET}/${BOARD}/vela_node.bin vela_node.bin

rm *.cc26x0-cc13x0 #remove some unused files

