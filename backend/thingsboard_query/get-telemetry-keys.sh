#!/bin/bash
#

if [ $# -eq 0 ]
  then
    echo "No argument found. The device ID should be supplied as first argument."
    exit 1
fi

if [ -z "$1" ]
  then
    echo "Invalid device ID"
    exit 1
fi

#DEVICE_ID=be988e50-f011-11e9-b2c8-f1eafb6162c5  #NODE: 02
#DEVICE_ID=6c5a8520-f012-11e9-b2c8-f1eafb6162c5	#NODE: 01
DEVICE_ID="$1"

JWT_TOKEN=$(./get-token.sh | jq -r '.token')

TB_RESPONSE=$(curl --fail --silent --show-error -X GET --header "Content-Type:application/json" --header "X-Authorization:Bearer $JWT_TOKEN" http://iot.smartcommunitylab.it/api/plugins/telemetry/DEVICE/${DEVICE_ID}/keys/timeseries)

echo ${TB_RESPONSE}
