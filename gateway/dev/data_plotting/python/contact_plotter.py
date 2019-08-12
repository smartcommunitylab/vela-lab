import matplotlib.pyplot as plt
import matplotlib.dates as matdates
from recordtype import recordtype
from datetime import datetime
import struct
import numpy as np
import math
readfile = open("vela-children/20190206-152830-contact.log", "r")
lines = readfile.read().splitlines()
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
    for j in range(len(nodes)):
        if nodes[j].nodeid == node_id:
            node_index =  j
    if node_index == -1:
        nodes.append(Node(node_id, [], [], [], [], []))
        x = len(nodes) - 1


    timestamp = datetime.strptime("{0} {1}".format(temp[0], temp[1]), "%Y-%m-%d %H:%M:%S")
    index = 0
    contacts = list()
    contactdata = temp[4]
    while index < len(contactdata):
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

for i in range(len(nodes)):
    if i == 0:
        unique_beacons = list(set(nodes[i].ids))
    else:
        unique_beacons.extend(list(set(nodes[i].ids)))

indexes_to_filter_out = list()
# Every ID that is true in the if statement below will be removed from the list in the next for loop and not be plotted
# Use this as a filter
# To plot all beacons comment out the indexes_to_filter_out.append statement
#for x in range(len(unique_beacons)):
    #if "0000000000" not in unique_beacons[x] or ("4" not in unique_beacons[x] and "5" not in unique_beacons[x]):
        #indexes_to_filter_out.append(x)
uniques = np.unique(unique_beacons)
print("AVAILABLE BEACONS (" + str(len(uniques)) + "):")
print(uniques)

show_only_this_beacon=["00000000005A"] # the filter does not work if more than one beacon is assigned
for x in range(len(unique_beacons)):
    for idf in show_only_this_beacon:
        if idf not in unique_beacons[x]:
            indexes_to_filter_out.append(x)
            
delcount = 0
for x in range(len(indexes_to_filter_out)):
    del unique_beacons[indexes_to_filter_out[x] - delcount]
    delcount += 1


print(unique_beacons)
unique_copies = list()

count = 0
ax1 = None
plines = []
figsize = [int(math.ceil(len(nodes)/2)), 2]
for y in range(len(nodes)):
    count += 1
    for x in range(len(unique_beacons)-1):
        #print("NEW LOOP")
        copy = Node(nodes[y].nodeid, nodes[y].timestamps.copy(), nodes[y].ids.copy(), nodes[y].lastRSSIs.copy(), nodes[y].maxRSSIs.copy(), nodes[y].pktCounters.copy())
        indexes_to_delete = list()

        for j in range(len(copy.ids)):
            if copy.ids[j] != unique_beacons[x]:
                indexes_to_delete.append(j)

        delCount = 0
        for j in range(len(indexes_to_delete)):
            del copy.timestamps[indexes_to_delete[j] - delCount]
            del copy.ids[indexes_to_delete[j] - delCount]
            del copy.lastRSSIs[indexes_to_delete[j] - delCount]
            del copy.maxRSSIs[indexes_to_delete[j] - delCount]
            del copy.pktCounters[indexes_to_delete[j] - delCount]
            delCount += 1
        #print(copy)
        if len(copy.ids) > 0:
            unique_copies.append(copy)
        indexes_to_delete.clear()

    #print(nodes[y])
    NUM_COLORS = len(nodes[y].ids)


    # fig = plt.figure()
    if ax1 is None:
        ax = plt.subplot(figsize[0], figsize[1], y + 1)
        ax1 = ax
    else:
        ax = plt.subplot(figsize[0], figsize[1], y + 1, sharey=ax1, sharex=ax1)

    ax.set_prop_cycle('color',plt.cm.Spectral(np.linspace(0,1,30)))
    # ax.set_prop_cycle('color', [cm(1.*i/NUM_COLORS) for i in range(NUM_COLORS)])
    for x in range(len(unique_copies)-1):
        dates = matdates.date2num(unique_copies[x].timestamps)
        plines.append(ax.plot_date(dates, unique_copies[x].maxRSSIs, '.-', label=str(unique_copies[x].ids[0])))

    # plt.legend(loc=3, bbox_to_anchor=(1, 0))
    ax.set_title("Node {0} - detected beacons".format(nodes[y].nodeid))

    plt.xlabel('Time (day, time)')
    plt.ylabel('RSSI')
    unique_copies.clear()

fig = plt.gcf()
plt.gcf().autofmt_xdate()
fig.set_size_inches(15, 9)

plt.show()
fig.savefig('samplefigure', bbox_extra_artists=(lgd,text), bbox_inches='tight')
