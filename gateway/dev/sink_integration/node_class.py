import threading
import time

class Node(object):
    def __init__(self, label, trickleCount=0):
        self.lock = threading.Lock()
        self.name = label
        self.lastTrickleCount = trickleCount
        self.lastMessageTime = float(time.time())
        self.batteryData=None
        self.amountOfBTReports = 0
        self.online=True
        self.fw_metadata=None
        self.nbr_string=""

    def updateTrickleCount(self,trickleCount):
        with self.lock:
            self.lastTrickleCount = trickleCount
            self.lastMessageTime = float(time.time())

    def updateBatteryData(self,batteryData):
        with self.lock:
            self.batteryData=batteryData

    def BTReportHandler(self):
        with self.lock:
            self.amountOfBTReports = self.amountOfBTReports + 1
            self.lastMessageTime = float(time.time())

    def updateFWMetadata(self, fw_metadata):
        with self.lock:
            self.fw_metadata=fw_metadata

    def getMetadataVersionString(self):
        if(self.fw_metadata!=None):
            fw_metadata_version_str = "{0}".format(hex(self.fw_metadata["version"]))
            return fw_metadata_version_str
        else:
            return ""

    def updateNbrString(self,nbr_string):
        with self.lock:
            self.nbr_string=nbr_string

    def getLastMessageElapsedTime(self):
        now = float(time.time())
        return now-self.lastMessageTime

    def resetNodeTimeout(self):
        with self.lock:
            self.lastMessageTime = float(time.time())
 
    def getNodeDescriptorObject(self):
        desc=dict()
        with self.lock:
            desc["battery"]=self.batteryData
            desc["firmware"]=self.fw_metadata
            desc["neighbors_info"]=self.nbr_string
            desc["online"]=self.online
            desc["number_of_bt_reports"]=self.amountOfBTReports
            
        return desc

    def printNodeInfo(self):
        if self.online:
            onlineMarker=' '
        else:
            onlineMarker='*'

        if self.batteryData != None and self.fw_metadata != None:
            print("| %3s%1s| %3.2fV %3.0f%% %4.0fmAh %6.1fmA  %5.1f°  |  %6.0f |   %5s  |   %3d | %6d | %54s |" % (str(self.name[-6:]), onlineMarker, self.batteryData["voltage"], self.batteryData["state_of_charge"], self.batteryData["capacity"], self.batteryData["consumption"], self.batteryData["temperature"], self.getLastMessageElapsedTime(), self.getMetadataVersionString(), self.lastTrickleCount, self.amountOfBTReports, self.nbr_string))
        else:
            print("| %3s |                                       |  %6.0f |          |   %3d | %6d |                                                      |" % (str(self.name[-6:]), self.getLastMessageElapsedTime(), self.lastTrickleCount, self.amountOfBTReports))
