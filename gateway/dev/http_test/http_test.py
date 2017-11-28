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


import requests
import json
import datetime


print("Creating url and header...")

url = 'https://climbdev.smartcommunitylab.it/v2/api/event/TEST/adca3db3-68d1-4197-b834-a45d61cf1c21/vlab'
# headers = {'Authorization': 'Bearer 831a2cc0-48bd-46ab-ace1-c24f767af8af', 'Content-Type': 'application/json'}
headers = {'Authorization': 'Bearer 831a2cc0-48bd-46ab-ace1-c24f767af8af'}

# timeNow = datetime.datetime.now()
# print("Timestamp:", timeNow)


payloadData = [{
    "wsnNodeId" : "Node01",
    "eventType" : 901,
    "timestamp" : 1511361257,
    "payload" : {
        "passengerId" : "a511a089-ab7a-446f-a8c2-4c208d4425c5",
        "latitude" : 46.0678106,
        "longitude" : 11.1515548,
        "accuracy" : 18.5380001068115
    }
}]


print("\nData:")
print(payloadData)


jsonData = json.dumps(payloadData)
print("\njsonData:")
print(jsonData)

r = requests.post(url, json=payloadData, headers=headers)
print("\nResponse:")
print(r.text)


# other request uses (not to use)

# r = requests.post(url, data=payloadData, headers=headers)
# print("\nResponse:")
# print(r.text)

# r = requests.post(url, data=jsonData, headers=headers)
# print("\nResponse:")
# print(r.text)
