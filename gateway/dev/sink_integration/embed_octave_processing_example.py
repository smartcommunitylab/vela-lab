import pexpect
import threading
import time
import os
from collections import deque
import json

octave_files_folder="../data_plotting/matlab_version" #./
contact_log_file_folder="log" #./
contact_log_filename="vela-09082019/20190809-141430-contact.log"
octave_launch_command="octave -qf"
detector_octave_script="run_detector.m"
events_file_json="json_events.txt"

PROCESS_INTERVAL=10*60
ENABLE_PROCESS_OUTPUT_ON_CONSOLE=True

class PROXIMITY_DETECTOR_THREAD(threading.Thread):
    def __init__(self, threadID, name):
        threading.Thread.__init__(self)
        self.threadID= threadID
        self.name = name
        self.__loop=True
        self.file_processed=True

    def get_result(self):
        with open(octave_files_folder+'/'+events_file_json) as f: # Use file to refer to the file object
            data=f.read()
            return json.loads(data)
        return None

    def on_processed(self):
        print("Contact log file processed!")
        self.file_processed=True
        #octave_process.kill(0)
        jdata=self.get_result()
        evts=jdata["proximity_events"]

        if evts!=None:
            self.on_events(evts)
            os.remove(octave_files_folder+'/'+events_file_json) #remove the file to avoid double processing of it

            evts=None
        else:
            print("No event foud!")
    
    def on_events(self,evts):
        print("Events: "+str(evts[0:10])) #NOTE: At this point the variable evts contains the proximity events. As example I plot the first 10

    def run(self):
        
        octave_process=None
        
        octave_to_log_folder_r_path=os.path.relpath(contact_log_file_folder, octave_files_folder)

        while self.__loop:
            try:
                if self.file_processed:
                    file_to_process=proximity_detector_in_queue.popleft()
                    self.file_processed=False

                    command=octave_launch_command+" "+detector_octave_script+" "+octave_to_log_folder_r_path+'/'+file_to_process
                    print("Sending this command: "+command)
                    octave_process=pexpect.spawnu(command,echo=False,cwd=octave_files_folder)

                if ENABLE_PROCESS_OUTPUT_ON_CONSOLE:
                    octave_process.expect("\n",timeout=60)
                    octave_return=octave_process.before.split('\r')
                    for s in octave_return:
                        if len(s)>0:
                            print("OCTAVE>>"+s)
                else:
                    octave_process.expect(pexpect.EOF,timeout=5)
                    self.on_processed()

            except pexpect.exceptions.TIMEOUT:
                print("Octave is processing...")
                pass
            except pexpect.exceptions.EOF:
                self.on_processed()
                pass
            except IndexError:
                time.sleep(1)
                pass

proximity_detector_in_queue=deque()
proximity_detector_thread=PROXIMITY_DETECTOR_THREAD(1,"proximity detection process")
proximity_detector_thread.setDaemon(True)
proximity_detector_thread.start()

while 1:

    proximity_detector_in_queue.append(contact_log_filename)
    print("Processing request queued...")
    time.sleep(PROCESS_INTERVAL) #the main thread does nothing

