#!/bin/bash
# redirect to the real build script

SOURCES_DIR=/home/giova/workspaces/GIT/vela-lab/network_board/integrate_sender_uart

# next line is necessary for generate debug symbols
export CFLAGS="-O0 -g"

cd ${SOURCES_DIR}
./build_script.sh "$@"
