#!/bin/bash
# script that sets the environmental variables and calls "make". Thi is to ease the build, anything to be done before make can be added here.
# All arguments passed to this script are forwarded to make call

export CONTIKI_ROOT=/Users/murphy/Projects/VelaLab/contiki

#export CONTIKI_ROOT=/home/giova/workspaces/GIT/contiki

while IFS=. read major minor build
do
VERSION_MAJOR=$major
VERSION_MINOR=$minor
VERSION_BUILD=$build
done < version

VERSION_BUILD=$((${VERSION_BUILD}+1))
export VERSION_STRING=$((VERSION_MAJOR)).$((VERSION_MINOR)).$((VERSION_BUILD))
echo $VERSION_STRING > version

make "$@" V=1 PORT=/dev/ttyACM1
