#!/bin/bash
# Note: for building the node use build_node_*.sh
# to clean run ./clean.sh

# ************************************************ CONFIGURATION ************************************************
CONTIKI_ROOT=../contiki-ng
BOARD=$1 #launchpad_vela/cc1350

echo "Building for: $BOARD" 
# ************************************************ BUILDING ************************************************
export CONTIKI_ROOT=${CONTIKI_ROOT}
export BOARD=${BOARD}
export TARGET=cc26x0-cc13x0
export CC26XX_UART_CONF_BAUD_RATE=1000000
export SERIAL_LINE_CONF_BUFSIZE=512
export UIP_CONF_BUFFER_SIZE=1024

make vela_sink V=1 PORT=/dev/ttyACM0 OTA=0 NODEID=0 SINK=1 CONTIKI_PROJECT=vela_sink "$@"

