#!/bin/bash
# script that sets the environmental variables and calls "make". This is to ease the build, anything to be done before make can be added here.
# Note: All arguments passed to this script are forwarded to make call

export NRF52_SDK_ROOT={absolute_path_to_nordic_sdk}

export NRF52_SDK_ROOT=/home/giova/workspaces/SDK/nRF5_SDK_14.0.0_3bcc1f7/

while IFS=. read major minor build
do
VERSION_MAJOR=$major
VERSION_MINOR=$minor
VERSION_BUILD=$build
done < version

VERSION_BUILD=$((${VERSION_BUILD}+1))
export VERSION_STRING=$((VERSION_MAJOR)).$((VERSION_MINOR)).$((VERSION_BUILD))
echo $VERSION_STRING > version

make "$@" VERBOSE=1 
