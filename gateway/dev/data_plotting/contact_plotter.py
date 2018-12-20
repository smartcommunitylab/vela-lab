import matplotlib.pyplot as plt
import matplotlib.dates as matdates
import numpy as np
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
# Or,
# ax.set_prop_cycle(color=[cm(1.*i/NUM_COLORS) for i in range(NUM_COLORS)])
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

# pnumdiff = list()
# pnumtimediff = list()
# for x in range(len(packetnumbers) - 1):
#     dif = int(packetnumbers[x + 1]) - int(packetnumbers[x])
#     if dif == -252:
#         dif = 1
#     if abs(dif) > 15 and packetnumbers[x + 1] != 1:
#         dif = 15
#     elif abs(dif) > 15 and packetnumbers[x + 1] == 1:
#         dif = -1
#     pnumdiff.append(dif)
#
#     tdif = packetnumbertimes[x + 1].timestamp() - packetnumbertimes[x].timestamp()
#     if tdif == 0:
#         tdif = 0.01
#     if dif == 0:
#         dif = 0.01
#     dtdif = dif / tdif
#     pnumtimediff.append(dtdif)
#
# packetnumdiff = np.array(pnumdiff)
# del packetnumbertimes[0]
# # del packetnumbertimes[len(packetnumbertimes)-1]
#
# print(splitlines[0])
#
# # ----- line structure -----
# # [0] Time
# # [1] Capacity
# # [2] State of Charge
# # [3] Estimated Lifetime
# # [4] Consumption
# # [5] Voltage
# # --------------------------
#
# all_time = list()
# all_capacity = list()
# all_soc = list()
# all_lifetime = list()
# all_consumption = list()
# all_voltage = list()
# all_temp = list()
#
# for x in range(len(splitlines)):
#     if len(splitlines[x]) > 1:
#         dt = datetime.strptime(splitlines[x][0], "%Y/%m/%d %H:%M:%S")
#         all_time.append(dt)
#         avgcap = splitlines[x][1].split()
#         all_capacity.append(float(avgcap[1]))
#         avgsoc = splitlines[x][2].split()
#         all_soc.append(float(avgsoc[3][:-1]))
#         avgcons = splitlines[x][4].split()
#         all_consumption.append(float(avgcons[2]))
#         avgvoltage = splitlines[x][5].split()
#         all_voltage.append(float(avgvoltage[3]))
#         avgtemp = splitlines[x][6].split()
#         all_temp.append(float(avgtemp[1]))
#
# plt.figure(figsize=(20, 15))
#
# dates = matdates.date2num(all_time)
#
# ax1 = plt.subplot(3, 2, 1)
# plt.title("Capacity")
# plt.plot_date(dates, all_capacity, 'bo')
# plt.xlabel('Time (day, time)')
# plt.ylabel('Capacity (mAh)')
# plt.gcf().autofmt_xdate()
# plt.tight_layout()
#
# ax2 = plt.subplot(3, 2, 2)
# plt.title("State of Charge")
# plt.plot_date(dates, all_soc, 'go')
# plt.gcf().autofmt_xdate()
# plt.xlabel('Time')
# plt.ylabel('State of Charge (%)')
# plt.tight_layout()
#
# ax3 = plt.subplot(3, 2, 3)
# plt.title("Net power-intake (mAh)")
# plt.plot_date(dates, all_consumption, 'ro')
# plt.gcf().autofmt_xdate()
# plt.xlabel('Time')
# plt.ylabel('Consumption (mA)')
# plt.tight_layout()
#
# ax4 = plt.subplot(3, 2, 4)
#
# plt.title("Battery voltage")
# plt.ylim(0, 4400)
# plt.plot_date(dates, all_voltage, 'co')
# plt.gcf().autofmt_xdate()
# plt.xlabel('Time')
# plt.ylabel('Battery voltage (mV)')
# plt.tight_layout()
#
# ax5 = plt.subplot(3, 2, 5)
# plt.title("Number of dropped packets")
# plt.ylim(-1, 15)
# plt.plot_date(packetnumbertimes, packetnumdiff, 'm-')
# plt.gcf().autofmt_xdate()
# plt.xlabel('Time')
# plt.ylabel('Lost Packets')
# plt.tight_layout()
#
# ax6 = plt.subplot(3, 2, 6)
# plt.title("Temperature")
# plt.plot_date(dates, all_temp, 'y-')
# plt.gcf().autofmt_xdate()
# plt.xlabel('Time')
# plt.ylabel('Temperature (*C)')
# plt.tight_layout()
#
# plt.setp(ax1.get_xticklabels(), visible=True)
# plt.setp(ax2.get_xticklabels(), visible=True)
# plt.setp(ax3.get_xticklabels(), visible=True)
# plt.setp(ax4.get_xticklabels(), visible=True)
#
# dateformatter = matdates.DateFormatter("%m-%d %H:%M")
# mdateformatter = matdates.DateFormatter("%S")
# ax1.xaxis.set_major_formatter(dateformatter)
# ax1.xaxis.set_minor_formatter(mdateformatter)
# ax2.xaxis.set_major_formatter(dateformatter)
# ax2.xaxis.set_minor_formatter(mdateformatter)
# ax3.xaxis.set_major_formatter(dateformatter)
# ax3.xaxis.set_minor_formatter(mdateformatter)
# ax4.xaxis.set_major_formatter(dateformatter)
# ax4.xaxis.set_minor_formatter(mdateformatter)
# ax5.xaxis.set_major_formatter(dateformatter)
# ax5.xaxis.set_minor_formatter(mdateformatter)
# ax6.xaxis.set_major_formatter(dateformatter)
# ax6.xaxis.set_minor_formatter(mdateformatter)
#
# fig = plt.figure()
# fig.autofmt_xdate()
#
# plt.show()
