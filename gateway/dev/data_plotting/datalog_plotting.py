import matplotlib.pyplot as plt
import matplotlib.dates as matdates
import numpy as np
from datetime import datetime

readfile = open("data.log", "r")
lines = readfile.readlines()
readfile.close()

splitlines = list()
packetnumbers = list()
packetnumbertimes = list()
for x in range(len(lines)):
    if 'INFO' in lines[x] and '[New message]' not in lines[x] and "[Node 135]" in lines[x]:
        line = lines[x].replace(',', '|', 1)
        line_list = line.split('|')
        t_info = line_list[0]
        l_t_info = t_info.split()
        line_list[0] = "{0} {1}".format(l_t_info[0], l_t_info[1])
        splitlines.append(line_list)

    elif '[New message]' in lines[x] and "nodeid 135" in lines[x]:
        line_list = lines[x].split()
        timestr = "{0} {1}".format(line_list[0], line_list[1])
        dt = datetime.strptime(timestr, "%Y/%m/%d %H:%M:%S")
        packetnumbertimes.append(dt)
        packetnumbers.append(int(line_list[len(line_list)-1]))

pnumdiff = list()
pnumtimediff = list()
for x in range(len(packetnumbers) - 1):
    dif = int(packetnumbers[x + 1]) - int(packetnumbers[x])
    if dif == -252:
        dif = 1
    if abs(dif) > 15 and packetnumbers[x + 1] != 1:
        dif = 15
    elif abs(dif) > 15 and packetnumbers[x + 1] == 1:
        dif = -1
    pnumdiff.append(dif)

    tdif = packetnumbertimes[x + 1].timestamp() - packetnumbertimes[x].timestamp()
    if tdif == 0:
        tdif = 0.01
    if dif == 0:
        dif = 0.01
    dtdif = dif / tdif
    pnumtimediff.append(dtdif)

packetnumdiff = np.array(pnumdiff)
del packetnumbertimes[0]
# del packetnumbertimes[len(packetnumbertimes)-1]

print(splitlines[0])

# ----- line structure -----
# [0] Time
# [1] Capacity
# [2] State of Charge
# [3] Estimated Lifetime
# [4] Consumption
# [5] Voltage
# --------------------------

all_time = list()
all_capacity = list()
all_soc = list()
all_lifetime = list()
all_consumption = list()
all_voltage = list()
all_temp = list()

for x in range(len(splitlines)):
    if len(splitlines[x]) > 1:
        dt = datetime.strptime(splitlines[x][0], "%Y/%m/%d %H:%M:%S")
        all_time.append(dt)
        avgcap = splitlines[x][1].split()
        all_capacity.append(float(avgcap[1]))
        avgsoc = splitlines[x][2].split()
        all_soc.append(float(avgsoc[3][:-1]))
        avgcons = splitlines[x][4].split()
        all_consumption.append(float(avgcons[2]))
        avgvoltage = splitlines[x][5].split()
        all_voltage.append(float(avgvoltage[3]))
        avgtemp = splitlines[x][6].split()
        all_temp.append(float(avgtemp[1]))

plt.figure(figsize=(20, 15))

dates = matdates.date2num(all_time)

ax1 = plt.subplot(3, 2, 1)
plt.title("Capacity")
plt.plot_date(dates, all_capacity, 'bo')
plt.xlabel('Time (day, time)')
plt.ylabel('Capacity (mAh)')
plt.gcf().autofmt_xdate()
plt.tight_layout()

ax2 = plt.subplot(3, 2, 2)
plt.title("State of Charge")
plt.plot_date(dates, all_soc, 'go')
plt.gcf().autofmt_xdate()
plt.xlabel('Time')
plt.ylabel('State of Charge (%)')
plt.tight_layout()

ax3 = plt.subplot(3, 2, 3)
plt.title("Net power-intake (mAh)")
plt.plot_date(dates, all_consumption, 'ro')
plt.gcf().autofmt_xdate()
plt.xlabel('Time')
plt.ylabel('Consumption (mA)')
plt.tight_layout()

ax4 = plt.subplot(3, 2, 4)

plt.title("Battery voltage")
plt.ylim(0, 4400)
plt.plot_date(dates, all_voltage, 'co')
plt.gcf().autofmt_xdate()
plt.xlabel('Time')
plt.ylabel('Battery voltage (mV)')
plt.tight_layout()

ax5 = plt.subplot(3, 2, 5)
plt.title("Number of dropped packets")
plt.ylim(-1, 15)
plt.plot_date(packetnumbertimes, packetnumdiff, 'm-')
plt.gcf().autofmt_xdate()
plt.xlabel('Time')
plt.ylabel('Lost Packets')
plt.tight_layout()

ax6 = plt.subplot(3, 2, 6)
plt.title("Temperature")
plt.plot_date(dates, all_temp, 'y-')
plt.gcf().autofmt_xdate()
plt.xlabel('Time')
plt.ylabel('Temperature (*C)')
plt.tight_layout()

plt.setp(ax1.get_xticklabels(), visible=True)
plt.setp(ax2.get_xticklabels(), visible=True)
plt.setp(ax3.get_xticklabels(), visible=True)
plt.setp(ax4.get_xticklabels(), visible=True)

dateformatter = matdates.DateFormatter("%m-%d %H:%M")
mdateformatter = matdates.DateFormatter("%S")
ax1.xaxis.set_major_formatter(dateformatter)
ax1.xaxis.set_minor_formatter(mdateformatter)
ax2.xaxis.set_major_formatter(dateformatter)
ax2.xaxis.set_minor_formatter(mdateformatter)
ax3.xaxis.set_major_formatter(dateformatter)
ax3.xaxis.set_minor_formatter(mdateformatter)
ax4.xaxis.set_major_formatter(dateformatter)
ax4.xaxis.set_minor_formatter(mdateformatter)
ax5.xaxis.set_major_formatter(dateformatter)
ax5.xaxis.set_minor_formatter(mdateformatter)
ax6.xaxis.set_major_formatter(dateformatter)
ax6.xaxis.set_minor_formatter(mdateformatter)

fig = plt.figure()
fig.autofmt_xdate()

plt.show()
