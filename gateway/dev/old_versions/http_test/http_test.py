# dalla mail di Michele:
# POST https://climbdev.smartcommunitylab.it/v2/api/event/TEST/adca3db3-68d1-4197-b834-a45d61cf1c21/vlab
# Header: "Authorization","Bearer 831a2cc0-48bd-46ab-ace1-c24f767af8af"
# Body:
# [
# {
#     "wsnNodeId" : "<id>",
#     "eventType" : 901,
#     "timestamp" : <timestamp>,
#     "payload" : {
#         "passengerId" : "a511a089-ab7a-446f-a8c2-4c208d4425c5",
#         "latitude" : 46.0678106,
#         "longitude" : 11.1515548,
#         "accuracy" : 18.5380001068115
#     }
# }
# ]

# contact data array formed by:
# {
#     "wsnNodeId" : <BeaconID> "string"
#     "eventType" : 901,
#     "timestamp" : <timestamp>,
#     "payload" : {
# 		"EndNodeID"   : "string"
# 		"lastRSSI"   : <int>
# 		"maxRSSI"  	 : <int>
# 		"pktCounter" : <int>
#     }
# }

# versione funzionante
# payloadData = [{
#     "wsnNodeId" : "Node01",
#     "eventType" : 901,
#     "timestamp" : 1511361257,
#     "payload" : {
#         "passengerId" : "a511a089-ab7a-446f-a8c2-4c208d4425c5",
#         "latitude" : 46.0678106,
#         "longitude" : 11.1515548,
#         "accuracy" : 18.5380001068115
#     }
# }]

# POST requests with data
# case 1: header no specified content type, request with json=payloadData where payloadData is python list and dict
# headers = {'Authorization': 'Bearer 831a2cc0-48bd-46ab-ace1-c24f767af8af'}
# fakeDataPayload = [
# {
#     "wsnNodeId" : "Beaconid_01",
#     "eventType" : EVENT_BECON_CONTACT,
#     "timestamp" : timeStart,
#     "payload" : {
# 		"EndNodeID": "VelaLab_EndNode_03",
# 		"lastRSSI": -30,
# 		"maxRSSI": -20,
# 		"pktCounter" : 15
#     }
# },
# {
#     "wsnNodeId" : "Beaconid_01",
#     "eventType" : EVENT_BECON_CONTACT,
#     "timestamp" : timeStart,
#     "payload" : {
# 		"EndNodeID" : "VelaLab_EndNode_04",
# 		"lastRSSI" : -31,
# 		"maxRSSI" : -21,
# 		"pktCounter" : 16
#     }
# }
# ]
#
# r = requests.post(urlDev, json=fakeDataPayload, headers=headers)

# case 2: header specified content type 'application/json', encode payloadData to jsonData (from python list and dict to json), request with data=jsonData
# headers = {'Authorization': 'Bearer 831a2cc0-48bd-46ab-ace1-c24f767af8af', 'Content-Type': 'application/json'}
# fakeDataPayload = [
# {
#     "wsnNodeId" : "Beaconid_01",
#     "eventType" : EVENT_BECON_CONTACT,
#     "timestamp" : timeStart,
#     "payload" : {
# 		"EndNodeID": "VelaLab_EndNode_03",
# 		"lastRSSI": -30,
# 		"maxRSSI": -20,
# 		"pktCounter" : 15
#     }
# },
# {
#     "wsnNodeId" : "Beaconid_01",
#     "eventType" : EVENT_BECON_CONTACT,
#     "timestamp" : timeStart,
#     "payload" : {
# 		"EndNodeID" : "VelaLab_EndNode_04",
# 		"lastRSSI" : -31,
# 		"maxRSSI" : -21,
# 		"pktCounter" : 16
#     }
# }
# ]
#
# jsonData = json.dumps(fakeDataPayload)
# r = requests.post(urlDev, data=jsonData, headers=headers)

import requests
import json
import time

EVENT_BECON_CONTACT = 901;

print("Creating url and header...")

urlDev_CLIMB = 'https://climbdev.smartcommunitylab.it/v2/api/event/TEST/adca3db3-68d1-4197-b834-a45d61cf1c21/vlab'

urlDev = 'https://climbdev.smartcommunitylab.it/v2/api/event/TEST/4220a8bb-3cf5-4076-b7bd-9e7a1ff7a588/vlab'
urlProd = ' https://climb.smartcommunitylab.it/v2/api/event/TEST/17ee8383-4cb0-4f58-9759-1d76a77f9eff/vlab'

headers = {'Authorization': 'Bearer 831a2cc0-48bd-46ab-ace1-c24f767af8af'}
# headers = {'Authorization': 'Bearer 831a2cc0-48bd-46ab-ace1-c24f767af8af', 'Content-Type': 'application/json'}

timeStart = int(time.time())
print("Timestamp :", timeStart)

lastRSSI = -30
maxRSSI = -20


fakeDataPayloadSingle = [{
    "wsnNodeId" : 'Beaconid_01',
    "eventType" : 901,
    "timestamp" : 112345,
    "payload" : {
		"EndNodeID": 'VelaLab_EndNode_05',
		"lastRSSI": -30,
		"maxRSSI": -20,
		"pktCounter" : 15
    }
}]

fakeDataPayload = [
{
    "wsnNodeId" : "Beaconid_01",
    "eventType" : EVENT_BECON_CONTACT,
    "timestamp" : timeStart,
    "payload" : {
		"EndNodeID": "VelaLab_EndNode_03",
		"lastRSSI": -30,
		"maxRSSI": -20,
		"pktCounter" : 15
    }
},
{
    "wsnNodeId" : "Beaconid_01",
    "eventType" : EVENT_BECON_CONTACT,
    "timestamp" : timeStart,
    "payload" : {
		"EndNodeID" : "VelaLab_EndNode_04",
		"lastRSSI" : -31,
		"maxRSSI" : -21,
		"pktCounter" : 16
    }
},
{
    "wsnNodeId" : "Beaconid_01",
    "eventType" : EVENT_BECON_CONTACT,
    "timestamp" : timeStart,
    "payload" : {
		"EndNodeID" : "VelaLab_EndNode_05",
		"lastRSSI" : -32,
		"maxRSSI" : -22,
		"pktCounter" : 17
    }
}
]


print("\nData:", fakeDataPayload)
r = requests.post(urlDev, json=fakeDataPayload, headers=headers)


# jsonData = json.dumps(fakeDataPayload)
# print("\njsonData:", jsonData)
# r = requests.post(urlDev, data=jsonData, headers=headers)

print("\nResponse:", r.text)




# other request uses (not to use)

# r = requests.post(url, data=payloadData, headers=headers)
# print("\nResponse:")
# print(r.text)

# r = requests.post(url, data=jsonData, headers=headers)
# print("\nResponse:")
# print(r.text)
