import params as par
import csv
import time
import matplotlib.pyplot as plt

def twos_comp(val, bits=8):
  val = int(val)
  """compute the 2's complement of int value val"""
  if (val & (1 << (bits - 1))) != 0: # if sign bit is set e.g., 8bit: 128-255
      val = val - (1 << bits)        # compute negative value
  return val   

def create_beacon_list(db):
  beacon_list = list()
  for contact_report in db:
    for beacon_contact in contact_report["beacon_contacts"]:
      if beacon_contact["id"] not in beacon_list:
        beacon_list.append(beacon_contact["id"])
  return beacon_list

def init_beacon_dict(beacon_list):
  ret_dict = dict()
  for beacon in beacon_list:
    ret_dict[beacon] = list()
  return ret_dict

log_filename = "23_dec_7-9.log"
db = list()
node_list = ["FD0000000000000002124B000F29CF01", "FD0000000000000002124B000BC9DD05", "FD0000000000000002124B000F29AB06", "FD0000000000000002124B001531FF86"]

min_ts = 1608705000000
max_ts = 1608708600000

if __name__ == "__main__":

  """ Here the code creates a dict with all the data """
  log_file = par.LOG_FOLDER_PATH + log_filename
  with open(log_file) as csv_file:
    csv_reader = csv.reader(csv_file, delimiter=',')
    for row in csv_reader:
      if (len(row)-3)%4 != 0:
        print("error")
      db.append({"ts": int(row[0]), "node": row[1], "beacon_contacts": list()})
      for i in range(2,len(row[3:]),4):
        db[-1]['beacon_contacts'].append({"id": row[i], "last_rssi": twos_comp(row[i+1]), "max_rssi": twos_comp(row[i+2]), "n_contacts": int(row[i+3])})

  beacon_list = create_beacon_list(db)
  beacon_dict = init_beacon_dict(beacon_list)

  for c_r in db:
    for contact in c_r["beacon_contacts"]:
      beacon_dict[contact["id"]].append({"ts":c_r["ts"], "node":c_r["node"], "last_rssi":contact["last_rssi"], "max_rssi":contact["max_rssi"]})

  """ plot single beacon different contacts """

  for bc in beacon_list:
    ts = dict()
    last_rssi = dict()

    plt.subplot((411))
    for i,node in enumerate(node_list):
      print(node)
      subplot_ix = 411 + i
      plt.subplot(subplot_ix)
      ts[node] = list()
      last_rssi[node] = list()
      for contact in beacon_dict[bc]:
        if contact["node"] == node:
          ts[node].append((contact["ts"] - min_ts)/(1000*60))
          last_rssi[node].append(contact["last_rssi"])
      plt.plot(ts[node], last_rssi[node])
      plt.xlim([0, (max_ts - min_ts)/(1000*60)])
      plt.ylim([-90, -50])
      plt.ylabel(node[-2:])
      plt.suptitle(bc)
      print(last_rssi[node])
    plt.savefig("./figure/{}".format(bc), dpi=500)
    plt.close()
