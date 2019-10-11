#!/bin/bash
# Cleans the project

# ************************************************ CONFIGURATION ************************************************
CONTIKI_ROOT=../contiki-ng
BOARD=launchpad_vela/cc1350

# ************************************************ BUILDING ************************************************
export CONTIKI_ROOT=${CONTIKI_ROOT}
export BOARD=${BOARD}
export TARGET=cc26x0-cc13x0

make clean

