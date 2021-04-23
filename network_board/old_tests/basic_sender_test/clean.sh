#!/bin/bash
# Cleans the project

# ************************************************ CONFIGURATION ************************************************
CONTIKI_ROOT=../contiki-ng
# ************************************************ BUILDING ************************************************

export CONTIKI_ROOT=${CONTIKI_ROOT}
export TARGET=cc26x0-cc13x0

if [ -z "$1" ] #when board argument is supplied, clean everithing
then
    # clean launchpad_vela/cc1350
    export BOARD=launchpad_vela/cc1350
    make clean

    # clean launchpad_vela/cc2650
    export BOARD=launchpad_vela/cc2650
    make clean

    # clean launchpad/cc1350
    export BOARD=launchpad/cc1350
    make clean

    # clean launchpad/cc2650
    export BOARD=launchpad/cc2650
    make clean

    # clean launchpad/cc1350
    export BOARD=sensortag/cc1350
    make clean

    # clean launchpad/cc2650
    export BOARD=sensortag/cc2650
    make clean
else
    export BOARD=$1 #launchpad_vela/cc1350

    length=$(($#))
    pass_through_agrs=${@:2:$length}

    make clean $pass_through_agrs
fi

