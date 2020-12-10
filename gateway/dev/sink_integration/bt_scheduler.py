import global_variables as g
import params as par
import threading
import datetime
import utils
import time

def should_be_on_bt(timestamp):
  ''' the function returns a boolean value if the actual time is between some predefined interval in the parameters file
  The function is used in the main loop to schedule the on and off period of bluetooth '''
  now = datetime.datetime.fromtimestamp(timestamp)

  # I copy only year, month and day from "now" variable
  date_on = [datetime.datetime(now.year, now.month, now.day, hour = par.on_hours[0], minute = par.on_minutes[0], second = par.on_seconds[0] ),\
             datetime.datetime(now.year, now.month, now.day, hour = par.on_hours[1], minute = par.on_minutes[1], second = par.on_seconds[1] )]
  
  date_off = [datetime.datetime(now.year, now.month, now.day, hour = par.off_hours[0], minute = par.off_minutes[0], second = par.off_seconds[0] ),\
              datetime.datetime(now.year, now.month, now.day, hour = par.off_hours[1], minute = par.off_minutes[1], second = par.off_seconds[1] )]

  if now < date_on[0]:
    return False
  elif now >= date_on[0] and now < date_off[0]:
    return True 
  elif now >= date_off[0] and now < date_on[1]:
    return False
  elif now >= date_on[1] and now < date_off[1]:
    return True
  else:
    return False

class BLUETOOTH_SCHEDULING_THREAD(threading.Thread):
    def __init__(self, threadID, name):
        threading.Thread.__init__(self)
        self.threadID= threadID
        self.name = name
        self.__loop=True
        self.forced = True
        self.is_bt_on = False
        time.sleep(30) # wait for the network to set up
    
    def run(self):
        while self.__loop:
            now = time.time()
            # g.net.addPrint("[BT SCHEDULER]: should be on is {} and self.is_bt_on is {}".format(should_be_on_bt(now), self.is_bt_on))            
            if should_be_on_bt(now) and (not self.is_bt_on):
                g.net.addPrint("[BT SCHEDULER]: Turning bluetooth on")
                g.net.sendNewTrickle(utils.build_outgoing_serial_message(par.PacketType.nordic_turn_bt_on, None),self.forced)
                self.is_bt_on = True
            elif (not should_be_on_bt(now)) and self.is_bt_on:
                g.net.addPrint("[BT SCHEDULER]: Turning bluetooth off")
                g.net.sendNewTrickle(utils.build_outgoing_serial_message(par.PacketType.nordic_turn_bt_off, None),self.forced)
                self.is_bt_on = False
            time.sleep(10)
