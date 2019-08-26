#!/bin/bash
# Note: for building the node use build_node_*.sh
# to clean run ./clean.sh

# ************************************************ CONFIGURATION ************************************************
CONTIKI_ROOT=/home/giova/workspaces/GIT/contiki-ng
#BOARD=launchpad_vela/cc1350
BOARD=launchpad_vela/cc1350

# ************************************************ BUILDING ************************************************
export CONTIKI_ROOT=${CONTIKI_ROOT}
export BOARD=${BOARD}
export TARGET=cc26x0-cc13x0
export CC26XX_UART_CONF_BAUD_RATE=57600
export SERIAL_LINE_CONF_BUFSIZE=512
export UIP_CONF_BUFFER_SIZE=1024

./copyForBuild.sh

while IFS=. read major minor build
do
VERSION_MAJOR=$major
VERSION_MINOR=$minor
VERSION_BUILD=$build
done < version

VERSION_BUILD=$((${VERSION_BUILD}+1))
export VERSION_STRING=$((VERSION_MAJOR)).$((VERSION_MINOR)).$((VERSION_BUILD))
echo $VERSION_STRING > version


make "$@" vela_sink V=0 PORT=/dev/ttyACM0 OTA=0 NODEID=0 SINK=1 CONTIKI_PROJECT=vela_sink
cp build/${TARGET}/${BOARD}/vela_sink.bin vela_sink.bin

rm *.cc26x0-cc13x0 #remove some unused files

