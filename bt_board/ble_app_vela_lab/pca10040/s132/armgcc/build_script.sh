#!/bin/bash
# script that sets the environmental variables and calls "make". This is to ease the build, anything to be done before make can be added here.
# Note: All arguments passed to this script are forwarded to make call

export NRF52_SDK_ROOT={absolute_path_to_nordic_sdk}

export NRF52_SDK_ROOT=/home/giova/workspaces/SDK/nRF5_SDK_14.0.0_3bcc1f7/

make "$@" VERBOSE=1
