#!/bin/bash
#

if [ $# -le 1 ]
  then
    echo "No enough arguments found. The device ID should be supplied as first argument, successive arguments are the keys."
    exit 1
fi

if [ -z "$1" ]
  then
    echo "Invalid device ID"
    exit 1
fi

DEVICE_ID="$1"

KEYS=""
i=0
for var in "${@}"
do
#   echo "$var"
  if [ $var != $DEVICE_ID ]
    then
      KEYS="${KEYS},${var}"
      KEYS_ARR[$i]="${var}"
      i=$i+1
    fi
done
KEYS=${KEYS:1}
#echo "KEYS are: $KEYS"
#echo "KEYS_ARR is: ${KEYS_ARR[@]}"

JWT_TOKEN=$(./get-token.sh | jq -r '.token')

CURL_OPTIONS="--fail --silent --show-error" # -v"
URL=https://iot.smartcommunitylab.it/api/plugins/telemetry/DEVICE/$DEVICE_ID/values/timeseries

DATE_UNDER_ANALYSIS=$(date +%D) #THIS IS FOR GETTING TODAY'S DATA
#DATE_UNDER_ANALYSIS=11/27/2019  #THIS IS FOR GETTING DATA FOR A SPECIFIC DAY: format mm/dd/yyyy

START_TIME=07:00:00
END_TIME=22:55:00

echo Analyzing date: ${DATE_UNDER_ANALYSIS} mm/dd/yyyy from ${START_TIME} to ${END_TIME}

START_TS=$((date --date="${DATE_UNDER_ANALYSIS} ${START_TIME}" +%s%N) | cut -b1-13)
END_TS=$((date --date="${DATE_UNDER_ANALYSIS} ${END_TIME}" +%s%N) | cut -b1-13)

#START_TS=1572562800000
#END_TS=1573513200000

TB_RESPONSE=$(curl ${CURL_OPTIONS} -X GET --header "Content-Type:application/json" --header "X-Authorization:Bearer $JWT_TOKEN" "${URL}?keys=${KEYS}&startTs=${START_TS}&endTs=${END_TS}&interval=0&limit=1000&agg=NONE")

#echo "${TB_RESPONSE}"

for k in ${KEYS_ARR[@]}
do
  evt_list=$(echo "$TB_RESPONSE" | jq .${k})
  #echo "evt_list: $evt_list"
  if [[ ${evt_list} != "null" ]]; then
    beaconName=$(echo "${k}" | tr -d "BID_")
    echo "beacon: ${beaconName[0]}, beacon key: ${k}"
    list_len=$(echo "$evt_list" | jq '. | length')
    for e_i in $(seq 0 $(($list_len-1)))
    do
      evt=$(echo "$evt_list" | jq ".[$e_i]")
      ts=$(echo "$evt" | jq '.ts')
      ts_string=$(date -d @$(($ts/1000)))
      value_s=$(echo "$evt" | jq '.value')
      value=$(echo "$value_s" | jq -rc '.')
      rssi=$(echo "$value" | jq '.rssi')
      t_start=$(echo "$value" | jq '.t_start')
      t_end=$(echo "$value" | jq '.t_end')
      t_start_str=$(date -d @$(($t_start/1000)))
      t_end_str=$(date -d @$(($t_end/1000)))
      evt_duration_s=$(($t_end/1000-$t_start/1000))
      #echo "   evtNo: $e_i => $ts_string -> rssi: ${rssi}dBm, event start: $t_start_str, event end: $t_end_str"
      echo "   evtNo: $e_i => $ts_string -> rssi: ${rssi}dBm, event duration: $evt_duration_s"
    done
  fi
done

exit 0

