#!/bin/bash
# script that sets the environmental variables and calls "make". Thi is to ease the build, anything to be done before make can be added here.
# All arguments passed to this script are forwarded to make call

export CONTIKI_ROOT={absolute_path_to_contiki}

export CONTIKI_ROOT=/home/giova/workspaces/GIT/contiki

make "$@" V=1 PORT=/dev/ttyUSB1
