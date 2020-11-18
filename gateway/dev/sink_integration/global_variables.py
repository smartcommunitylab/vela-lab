from recordtype import recordtype


mqtt_client = None
mqtt_connected=False

n_chunks_in_firmware = None

dropCounter = 0
NodeDropInfo = recordtype("NodeDropInfo", "nodeid, lastpkt")
nodeDropInfoList = []

MessageSequence = recordtype("MessageSequence","timestamp,nodeid,lastPktnum,sequenceSize,datacounter,datalist,latestTime")
messageSequenceList = []

ContactData = recordtype("ContactData", "nodeid lastRSSI maxRSSI pktCounter")
