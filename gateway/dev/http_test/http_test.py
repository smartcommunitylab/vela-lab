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



import requests
import json
import time

EVENT_BECON_CONTACT = 901;

print("Creating url and header...")

urlDev_CLIMB = 'https://climbdev.smartcommunitylab.it/v2/api/event/TEST/adca3db3-68d1-4197-b834-a45d61cf1c21/vlab'

urlDev = 'https://climbdev.smartcommunitylab.it/v2/api/event/TEST/4220a8bb-3cf5-4076-b7bd-9e7a1ff7a588/vlab'
urlProd = ' https://climb.smartcommunitylab.it/v2/api/event/TEST/17ee8383-4cb0-4f58-9759-1d76a77f9eff/vlab'
# headers = {'Authorization': 'Bearer 831a2cc0-48bd-46ab-ace1-c24f767af8af', 'Content-Type': 'application/json'}

headers = {'Authorization': 'Bearer 831a2cc0-48bd-46ab-ace1-c24f767af8af'}

# timeNow = datetime.datetime.now()
# print("Timestamp:", timeNow)

timestamp = int(time.time())
print("Timestamp :", timestamp)

lastRSSI = -30
maxRSSI = -20


payloadDataS = [
{
    "wsnNodeId" : "Beaconid_01",
    "eventType" : 901,
    "timestamp" : timestamp,
    "payload" : {
		"EndNodeID": "VelaLab_EndNode_05",
		"lastRSSI": lastRSSI,
		"maxRSSI": maxRSSI,
		"pktCounter" : 30
    }
}
]

payloadData = [
{
    "wsnNodeId" : "Beaconid_01",
    "eventType" : EVENT_BECON_CONTACT,
    "timestamp" : timestamp,
    "payload" : {
		"EndNodeID": "VelaLab_EndNode_05",
		"lastRSSI": lastRSSI,
		"maxRSSI": maxRSSI,
		"pktCounter" : 30
    }
},
{
    "wsnNodeId" : "Beaconid_01",
    "eventType" : EVENT_BECON_CONTACT,
    "timestamp" : timestamp,
    "payload" : {
		"EndNodeID" : "VelaLab_EndNode_05",
		"lastRSSI" : lastRSSI,
		"maxRSSI" : maxRSSI,
		"pktCounter" : 20
    }
},
{
    "wsnNodeId" : "Beaconid_01",
    "eventType" : EVENT_BECON_CONTACT,
    "timestamp" : timestamp,
    "payload" : {
		"EndNodeID" : "VelaLab_EndNode_03",
		"lastRSSI" : lastRSSI,
		"maxRSSI" : maxRSSI,
		"pktCounter" : 14
    }
}
]


print("\nData:")
print(payloadData)


jsonData = json.dumps(payloadData)
print("\njsonData:")
print(jsonData)

r = requests.post(urlDev_CLIMB, json=payloadDataS, headers=headers)
print("\nResponse:")
print(r.text)


# other request uses (not to use)

# r = requests.post(url, data=payloadData, headers=headers)
# print("\nResponse:")
# print(r.text)

# r = requests.post(url, data=jsonData, headers=headers)
# print("\nResponse:")
# print(r.text)
