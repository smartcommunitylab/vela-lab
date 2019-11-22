#!/bin/bash
#

#DEVICE_ID=6c5a8520-f012-11e9-b2c8-f1eafb6162c5	#NODE 1
#DEVICE_ID=be988e50-f011-11e9-b2c8-f1eafb6162c5	#NODE 2
#DEVICE_ID=bac406b0-f011-11e9-b2c8-f1eafb6162c5	#NODE 3
DEVICE_ID=0bd70e40-f0de-11e9-b2c8-f1eafb6162c5	#NODE 4
#DEVICE_ID=bd4385a0-f011-11e9-b2c8-f1eafb6162c5	#NODE 5
#DEVICE_ID=5e878c90-f012-11e9-b2c8-f1eafb6162c5	#NODE 6

AVAILABLE_KEYS="$(./get-telemetry-keys.sh ${DEVICE_ID})"

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

#echo ${AVAILABLE_BEACONS[@]}

i=0
for b in ${AVAILABLE_BEACONS[@]}
do
#  echo "${AVAILABLE_BEACONS[$i]}"
  i=$i+1
done

#TODO: in pseudocode
./get-telemetry.sh ${DEVICE_ID} ${AVAILABLE_BEACONS[@]}
