#!/bin/bash
# Note: for building the node use build_node_*.sh
# to clean run ./clean.sh
# This script expects the BOARD as fist argument (for example ./build_sink.sh launchpad_vela/cc1350)
# ************************************************ CONFIGURATION ************************************************
CONTIKI_ROOT=../contiki-ng
BOARD=$1 #launchpad_vela/cc1350

echo "Building for: $BOARD" 
# ************************************************ BUILDING ************************************************
export CONTIKI_ROOT=${CONTIKI_ROOT}
export BOARD=${BOARD}
export TARGET=cc26x0-cc13x0
export CC26XX_UART_CONF_BAUD_RATE=57600
export SERIAL_LINE_CONF_BUFSIZE=512
export UIP_CONF_BUFFER_SIZE=1024

MY_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"  #give you the full directory name of the script no matter where it is being called from.
cd $MY_DIR  #make sure we are in the proper directory

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


make vela_sink V=1 PORT=/dev/ttyACM0 OTA=0 NODEID=0 SINK=1 CONTIKI_PROJECT=vela_sink "$@"
cp build/${TARGET}/${BOARD}/vela_sink.bin vela_sink.bin

rm *.cc26x0-cc13x0 #remove some unused files

