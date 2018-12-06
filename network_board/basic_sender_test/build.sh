#!/bin/bash
# script that sets the environmental variables and calls "make". Thi is to ease the build, anything to be done before make can be added here.
# All arguments passed to this script are forwarded to make call
#If device detection is unavailable, the script will try to upload to the default port /dev/ttyACM0

export CONTIKI_ROOT={absolute_path_to_contiki}

export CONTIKI_ROOT=/home/bram/Documents/ContikiTest/contiki


#Device detection rootdirectory for devices detection script
device_detection_root=../../../DeviceDetection
detect_command=detect-devices.sh

while IFS=. read major minor build
do
VERSION_MAJOR=$major
VERSION_MINOR=$minor
VERSION_BUILD=$build
done < version

VERSION_BUILD=$((${VERSION_BUILD}+1))
export VERSION_STRING=$((VERSION_MAJOR)).$((VERSION_MINOR)).$((VERSION_BUILD))
echo $VERSION_STRING > version

# next line is necessary for generate debug symbols
export CFLAGS="-O0 -g"

make "$@" V=1 PORT=/dev/ttyACM0

#if [ -f ${device_detection_root}/detect-devices.sh ]; then
#    echo "Detect devices script found!"
#    
#    pwd
#    mapfile -t connected_devices < <( source ${device_detection_root}/detect-devices.sh )
#    declare -p  connected_devices    
#    
#    touch greptmp
#    read -ra input_line_array <<<"$@"
#    
#    count=0
#	for i in "${input_line_array[@]}"
#	do
#    	command=$( echo $i | cut -c1-6 )
#    	#split until character 20 to make sure every character of the serial number is included
#    	device=$( echo $i | cut -c8-20 )
#    	if [ "$command" = "SERIAL" ]; then
#    		#find which port device is connected to
#    		for j in "${connected_devices[@]}"
#			do
#   				found_serial=$( echo "$j" | cut -d' ' -f 7)
#   				if [ "$device" = "$found_serial" ]; then
#   					echo "Found a target"
#   					target_port=$( echo "$j" | cut -d' ' -f 8)
#   					make_input=""
#   					echo STARTING LOOP WITH COUNT: "$count"
#   					countl=$(($count-1))
#   					for k in `seq 0 $countl`;
#        			do
#                		make_input="$make_input "${input_line_array[$k]}""
#       				done    
#       				echo "MAKE INPUT: $make_input"
#   					make $make_input V=1 PORT=$target_port
#   				fi
#			done
#		else
#			#count how many non SERIAL commands are provided
#    		count=$(($count+1))
#    	fi
#    done	
#    
#     
#else
#	echo "Detect devices script not found, uploading to default port"
#fi
