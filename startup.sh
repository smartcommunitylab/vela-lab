#!/bin/bash

PYTHON_SCRIPT_ABSOLUTE_PATH=/home/pi/Desktop/vela-lab/gateway/dev/sink_integration

cd $PYTHON_SCRIPT_ABSOLUTE_PATH
python3 main.py /dev/ttyS0 57600
