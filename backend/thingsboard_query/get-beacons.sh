#!/bin/bash
#

if [ $# == 1 ]; then
  re='^[0-9]+$'
  if ! [[ $1 =~ $re ]] ; then
    echo "DEVICE_IDX is not a number!"
    exit 1
  fi
  DEVICE_IDX="$1"
else
  DEVICE_IDX=3  #default value for DEVICE_IDX
fi

# The mapping of Device name and device id is done on the Thingsboard->Devices web interface (https://iot.smartcommunitylab.it/devices)

i=0
DEVICE_ID[$i]=6c5a8520-f012-11e9-b2c8-f1eafb6162c5	#NODE 1
DEVICE_NAME[$i]=FD0000000000000002124B000F29CF01
i=$i+1

DEVICE_ID[$i]=be988e50-f011-11e9-b2c8-f1eafb6162c5	#NODE 2
DEVICE_NAME[$i]=FD0000000000000002124B000BCAA602
i=$i+1

DEVICE_ID[$i]=bac406b0-f011-11e9-b2c8-f1eafb6162c5	#NODE 3
DEVICE_NAME[$i]=FD0000000000000002124B000F29A603
i=$i+1

DEVICE_ID[$i]=0bd70e40-f0de-11e9-b2c8-f1eafb6162c5	#NODE 4
DEVICE_NAME[$i]=FD0000000000000002124B000BCB2604
i=$i+1

DEVICE_ID[$i]=bd4385a0-f011-11e9-b2c8-f1eafb6162c5	#NODE 5
DEVICE_NAME[$i]=FD0000000000000002124B000BC9DD05
i=$i+1

DEVICE_ID[$i]=5e878c90-f012-11e9-b2c8-f1eafb6162c5	#NODE 6
DEVICE_NAME[$i]=FD0000000000000002124B000F29AB06
i=$i+1

#echo ${DEVICE_NAME[@]}


i_d=0
for d in ${DEVICE_ID[@]}
do
  AVAILABLE_KEYS="$(./get-telemetry-keys.sh ${DEVICE_ID[$i_d]})"

  #echo AVAILABLE_KEYS=${AVAILABLE_KEYS}

  KEYS=$(echo ${AVAILABLE_KEYS} | tr "\"" "\n")


  #AVAILABLE_BEACONS=()
  i=0
  for key in ${KEYS}
  do
    if [[ $key == "BID_"* ]]; then
      AVAILABLE_BEACONS[$i]="$key"
      i=$i+1
    fi
  done

#echo ${AVAILABLE_BEACONS[@]

  echo "${DEVICE_NAME[$i_d]}" 
  ./get-telemetry.sh ${d} ${AVAILABLE_BEACONS[@]}
  i_d=$i_d+1
  echo ""
done
