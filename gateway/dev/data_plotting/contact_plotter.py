import matplotlib.pyplot as plt
import matplotlib.dates as matdates
from recordtype import recordtype
from datetime import datetime
import struct

readfile = open("contact.log", "r")
lines = readfile.readlines()
readfile.close()

Node = recordtype("Nodes", "nodeid, timestamps, ids, lastRSSIs, maxRSSIs, pktCounters")
nodes = list()

Contact = recordtype("Contact", "")

for x in range(len(lines)):
    temp = lines[x].split(" ")
    node_id = temp[3]

    node_index = -1
    if len(nodes) == 0:
        nodes.append(Node(node_id, [], [], [], [], []))
    for x in range(len(nodes)):
        if nodes[x].nodeid == node_id:
            node_index =  x
    if x == -1:
        nodes.append(Node(node_id, [], []))
        x = len(nodes) - 1


    timestamp = datetime.strptime("{0} {1}".format(temp[0], temp[1]), "%Y-%m-%d %H:%M:%S")
    index = 0
    contacts = list()
    contactdata = temp[4]
    print(contactdata[index:index + 1])
    while index < len(contactdata) - 1:
        id = contactdata[index:index+12]
        index += 12
        lastRSSI = struct.unpack('>b', bytearray.fromhex(contactdata[index:index + 2]))[0]
        index += 2
        maxRSSI = struct.unpack('>b', bytearray.fromhex(contactdata[index:index + 2]))[0]
        index += 2
        pktCounter = int(contactdata[index:index+2], 16)
        index+=2
        nodes[node_index].timestamps.append(timestamp)
        nodes[node_index].ids.append(id)
        nodes[node_index].lastRSSIs.append(lastRSSI)
        nodes[node_index].maxRSSIs.append(maxRSSI)
        nodes[node_index].pktCounters.append(pktCounter)

unique_nodes = list(set(nodes[0].ids))
print(unique_nodes)
unique_copies = list()

for x in range(len(unique_nodes)):
    print("NEW LOOP")
    copy = Node(nodes[0].nodeid, nodes[0].timestamps.copy(), nodes[0].ids.copy(), nodes[0].lastRSSIs.copy(), nodes[0].maxRSSIs.copy(), nodes[0].pktCounters.copy())
    indexes_to_delete = list()


    for j in range(len(copy.ids)):
        if copy.ids[j] != unique_nodes[x]:
            indexes_to_delete.append(j)

    delCount = 0
    for j in range(len(indexes_to_delete)):
        del copy.timestamps[indexes_to_delete[j] - delCount]
        del copy.ids[indexes_to_delete[j] - delCount]
        del copy.lastRSSIs[indexes_to_delete[j] - delCount]
        del copy.maxRSSIs[indexes_to_delete[j] - delCount]
        del copy.pktCounters[indexes_to_delete[j] - delCount]
        delCount += 1
    print(copy)
    unique_copies.append(copy)
    indexes_to_delete.clear()



print(nodes[0])
ax1 = plt.subplot(1,1,1)
plt.title("Node 2 - detected endnodes")
NUM_COLORS = len(unique_nodes)

cm = plt.get_cmap('gist_rainbow')
fig = plt.figure()
ax = fig.add_subplot(111)
ax.set_prop_cycle('color', [cm(1.*i/NUM_COLORS) for i in range(NUM_COLORS)])
for x in range(len(unique_copies)):
    dates = matdates.date2num(unique_copies[x].timestamps)
    plt.plot_date(dates, unique_copies[x].lastRSSIs, '-', label=str(unique_nodes[x]))
plt.legend(loc=3, bbox_to_anchor=(1, -0.1))
plt.xlabel('Time (day, time)')
plt.ylabel('Capacity (mAh)')
fig = plt.gcf()
plt.gcf().autofmt_xdate()
fig.set_size_inches(10, 5)
plt.show()
