from itertools import chain, starmap
import global_variables as g
import network_params as net_par

def flatten_json(dictionary):
    """Flatten a nested json file"""

    def unpack(parent_key, parent_value):
        """Unpack one level of nesting in json file"""
        # Unpack one level only!!!
        
        if isinstance(parent_value, dict):
            for key, value in parent_value.items():
                temp1 = parent_key + '_' + key
                yield temp1, value
        elif isinstance(parent_value, list):
            i = 0 
            for value in parent_value:
                temp2 = parent_key + '_'+str(i) 
                i += 1
                yield temp2, value
        else:
            yield parent_key, parent_value    

            
    # Keep iterating until the termination condition is satisfied
    while True:
        # Keep unpacking the json file until all values are atomic elements (not dictionary or list)
        dictionary = dict(chain.from_iterable(starmap(unpack, dictionary.items())))
        # Terminate condition: not any value in the json file is dictionary or list
        if not any(isinstance(value, dict) for value in dictionary.values()) and \
           not any(isinstance(value, list) for value in dictionary.values()):
            break

    return dictionary

def decode_node_info(line):
  # TODO change the way pktnum's are being tracked
  cursor = 0
  source_addr_bytes=line[cursor:cursor+16]
  nodeid_int = int.from_bytes(source_addr_bytes, "big", signed=False) 
  nodeid='%(nodeid_int)X'%{"nodeid_int": nodeid_int}
  cursor+=16
  pkttype = int(line[cursor:cursor+2].hex(),16)
  cursor+=2
  pktnum = int.from_bytes(line[cursor:cursor+1], "little", signed=False) 
  cursor+=1
  return cursor, nodeid, pkttype, pktnum

""" check the number of dropped packages during the whole life of the network """
def check_for_packetdrop(nodeid, pktnum):
    if pktnum == 0:
        return
    for x in range(len(g.nodeDropInfoList)):
        if g.nodeDropInfoList[x].nodeid == nodeid:
            g.dropCounter += pktnum - g.nodeDropInfoList[x].lastpkt - 1
            return
    g.nodeDropInfoList.append(g.NodeDropInfo(nodeid, pktnum))

def get_sequence_index(nodeid):
    for x in range(len(g.messageSequenceList)):
        if g.messageSequenceList[x].nodeid == nodeid:
            return x
    return -1

def decode_payload(payload, seqid, size, pktnum): #TODO: packettype can be removed
    cur=0
    try:
        for x in range(round(size / net_par.SINGLE_NODE_REPORT_SIZE)):
            nid = int(payload[cur:cur+12], 16)
            cur=cur+12
            lastrssi = int(payload[cur:cur+2], 16)
            cur=cur+2
            maxrssi = int(payload[cur:cur+2], 16)
            cur=cur+2
            pktcounter = int(payload[cur:cur+2], 16)
            cur=cur+2
            contact = g.ContactData(nid, lastrssi, maxrssi, pktcounter)
            g.messageSequenceList[seqid].datalist.append(contact)
            g.messageSequenceList[seqid].datacounter += net_par.SINGLE_NODE_REPORT_SIZE

    except ValueError:
        print("[Node {0}] Requested to decode more bytes than available. Requested: {1}"
                          .format(g.messageSequenceList[seqid].nodeid, size))
    finally:
        return