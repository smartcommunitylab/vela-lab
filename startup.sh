#!/bin/bash

MY_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"  #give you the full directory name of this script no matter where it is being called from.
cd $MY_DIR  #make sure we are in the proper directory

PYTHON_SCRIPT_PATH=./gateway/dev/sink_integration

cd $PYTHON_SCRIPT_PATH
python3 main.py $@
