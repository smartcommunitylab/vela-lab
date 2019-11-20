function proximity_events_json=proximity_detect(filename, text_verbosity, image_verbosity, id_to_analyze, id_to_remove)

  arg_list=argv();

  addpath("jsonlab/")

  if nargin<4
    id_to_analyze={}
  endif
  if nargin<5
    id_to_remove={}
  endif
  
  VERBOSITY_OFF=0;
  VERBOSITY_INFO=1;
  VERBOSITY_REPORT=2;

  PLOT_DISABLED=0;
  PLOT_ENABLED=1;
  % set ID_TO_ANALYZE to empty to remove the filter
  ID_TO_ANALYZE=id_to_analyze;

  ID_TO_REMOVE=id_to_remove;

  if image_verbosity>PLOT_DISABLED
    ENABLE_PLOT=true;       %whether or not to show plot (the text output is always shown)
  else
    ENABLE_PLOT=false;
  endif
  %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

  #filename="vela-09082019/20190809-141430-contact_cut.log";
  #filename="vela-09082019/20190809-141430-contact.log";

  %% Open the text file.
  fileID = fopen(filename,'r');
  %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

  if text_verbosity>=VERBOSITY_INFO
    printf("Importing file...\n");
  endif
  formatSpec = "%s";
  dataArray = textscan(fileID, formatSpec, -1,"Whitespace","","Delimiter","","EndOfLine",'\n');
  fclose(fileID);
  if text_verbosity>=VERBOSITY_INFO
    printf("File imported.\n");
  endif
  %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
  text_preallocation_size=100000;
  
  if text_verbosity>=VERBOSITY_INFO
    printf("Parsing packets...\n");
  endif
  log_date=zeros(text_preallocation_size,3);
  log_time=zeros(text_preallocation_size,3);
  log_origin=zeros(text_preallocation_size,1);
  log_origin_fullname=cell(text_preallocation_size,1);
  log_data=cell(text_preallocation_size,1);
  valid_line=1;
  lines_to_scan=size(dataArray{1},1);
  if lines_to_scan > text_preallocation_size
    warning("Preallocation size isn't enough. To improve performance increase text_preallocation_size to: %d",lines_to_scan)
  endif

  for lineNo=1:lines_to_scan
    #lineNo
    line_=dataArray{1}(lineNo,1);
    
    [val, count, errmsg, pos] = sscanf(line_{1},"%d-%d-%d %d:%d:%d Node %s %s");
    YYYY=val(1,1);
    MM=val(2,1);
    DD=val(3,1);
    
    hh=val(4,1);
    mm=val(5,1);
    ss=val(6,1);
    
    data=char(val(7:end,1))';
    
    nodeID_len=32;
    nodeID_full = data(1,1:nodeID_len);
    nodeID_trunksize=6;
    nodeID=hexstr2int(nodeID_full(1,end-nodeID_trunksize+1:end));
    
    origin=nodeID;
    hex_str=data;
    
    log_date(valid_line,:)=[YYYY, MM, DD];
    log_time(valid_line,:)=[hh,mm,ss];
    log_origin(valid_line,1)=origin;
    log_origin_fullname{valid_line,1}=nodeID_full;
    log_data{valid_line,1}=data(1,nodeID_len+1:end);
    
    valid_line=valid_line+1;
    if text_verbosity>=VERBOSITY_INFO
      if(mod(lineNo,100)==0)
        percent=lineNo/lines_to_scan*100;
        printf("\r%.2f percent parsed...",percent);
      endif
    endif
  endfor
  log_date=log_date(1:valid_line-1,:);
  log_time=log_time(1:valid_line-1,:);
  log_origin=log_origin(1:valid_line-1,:);
  log_origin_fullname=log_origin_fullname(1:valid_line-1,:);
  log_data=log_data(1:valid_line-1,:);
  
  if text_verbosity>=VERBOSITY_INFO
    printf("\nPackets parsed.\n");
  endif
  %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
  if text_verbosity>=VERBOSITY_INFO
    printf("Pre analisys...\n");
  endif
  EXPECTED_AMOUNT_OF_NODES=4;

  packet_preallocation_size=100000;

  scanners={};
  nodesIdxes=[];  %this is used to ease the search of the index, otherwise it should iterate over scanners.scannerIdx
  reports_to_analyze=size(log_data,1);
  for reportNo=1:reports_to_analyze
    if size(log_data{reportNo,1},2)>14
        usefull_part = log_data{reportNo};
        
        #offset=1;
        
        nodeID_len=32;
        nodeID=log_origin(reportNo);
        nodeID_full=log_origin_fullname{reportNo,1};
        #offset=offset+nodeID_len;
        
        pkt_type_len=4;
        #offset=offset+pkt_type_len;
        
        pkt_num_len=2;
        #offset=offset+pkt_num_len;
        
        contactsData=usefull_part; #(1,offset:end);
        #offset=offset+nodeID_len;
        
        scannerIdx=find(nodesIdxes==nodeID);
        if isempty(scannerIdx)
            scannerIdx=size(nodesIdxes,1)+1;
            scanners{scannerIdx,1}.nodeID=nodeID;
            scanners{scannerIdx,1}.nodeID_full=nodeID_full;
            scanners{scannerIdx,1}.noOfReports=0;
            nodesIdxes(scannerIdx,1)=nodeID;
            scanners{scannerIdx,1}.noOfContacts=0;
        endif

        scanners{scannerIdx,1}.noOfReports=scanners{scannerIdx,1}.noOfReports+1;
        cur = 1;
        while cur < size(contactsData,2)
            beaconID=contactsData(cur:cur+11);
            cur=cur+18;

            isBeaconValid = (isempty(ID_TO_ANALYZE) || sum(strcmp(ID_TO_ANALYZE,beaconID))) && (isempty(ID_TO_REMOVE) || ~sum(strcmp(ID_TO_REMOVE,beaconID)) );
            if isBeaconValid
                scanners{scannerIdx}.noOfContacts=scanners{scannerIdx}.noOfContacts+1;
            endif
        endwhile
        
    else
        warning('Short line found: %s',log_data{reportNo});
    endif
    
    if text_verbosity>=VERBOSITY_INFO
      if(mod(reportNo,100)==0)
        percent=reportNo/reports_to_analyze*100;
        printf("\r%.2f percent analyzed...",percent);
      endif
    endif
  endfor
  noOfScanners=size(scanners,1);
  if text_verbosity>=VERBOSITY_INFO
    printf("Pre analysis done.\n");
  endif
  %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
  if text_verbosity>=VERBOSITY_INFO
    printf("Extracting contacts data...\n");
  endif
  unique_filtered=[];
  for scannerIdx=1:size(scanners,1)
      selected_idx = log_origin==scanners{scannerIdx,1}.nodeID;
      
      epoch=datenum(1970, 1, 1, 0, 0, 0);
      gmtoffset_s=localtime(now()).gmtoff; #might be not working (on windowns gmtoff=0, on ubunut gmtoff=3600 but should be 7200 because of daylight saving time)
      timestamps=datenum(log_date(selected_idx,1), log_date(selected_idx,2), log_date(selected_idx,3), log_time(selected_idx,1), log_time(selected_idx,2), log_time(selected_idx,3))-epoch-(gmtoffset_s/60/24); # - zero_t;
      
      if scannerIdx==1
        zero_t=timestamps(1);
      endif     
      
      contactsData=cell(scanners{scannerIdx,1}.noOfReports,1);

      i=find(selected_idx);
      for c=1:size(i,1)
        contactsData{c,1} = log_data{i(c,1)};
      endfor
      
      scanners{scannerIdx}.noOfReports = sum(selected_idx); #should be the same number
      
      scanners{scannerIdx}.t=NaN(scanners{scannerIdx}.noOfContacts,1);
      scanners{scannerIdx}.beaconID=cell(scanners{scannerIdx}.noOfContacts,1);
      scanners{scannerIdx}.lastRSSI=zeros(scanners{scannerIdx}.noOfContacts,1);
      scanners{scannerIdx}.maxRSSI=zeros(scanners{scannerIdx}.noOfContacts,1);
      scanners{scannerIdx}.contactCounter=zeros(scanners{scannerIdx}.noOfContacts,1);
      bidx = 1;
      
      for reportNo = 1:scanners{scannerIdx}.noOfReports
      
          cur = 1;
          timestamp=timestamps(reportNo);
          while cur < size(contactsData{reportNo},2)
              beaconID=contactsData{reportNo}(cur:cur+11);
              cur=cur+12;
              
              lastRSSI=typecast(uint8(sscanf(contactsData{reportNo}(cur:cur+1),'%x')), 'int8');
              cur=cur+2;
              maxRSSI=typecast(uint8(sscanf(contactsData{reportNo}(cur:cur+1),'%x')), 'int8');
              cur=cur+2;
              contactCounter=uint8(hexstr2int(contactsData{reportNo}(cur:cur+1)));
              cur=cur+2;

              isBeaconValid = (isempty(ID_TO_ANALYZE) || sum(strcmp(ID_TO_ANALYZE,beaconID))) && (isempty(ID_TO_REMOVE) || ~sum(strcmp(ID_TO_REMOVE,beaconID)) );
              if isBeaconValid
                  scanners{scannerIdx}.t(bidx,1)=timestamp;
                  scanners{scannerIdx}.beaconID{bidx,1}=beaconID;
                  scanners{scannerIdx}.lastRSSI(bidx,1)=lastRSSI;
                  scanners{scannerIdx}.maxRSSI(bidx,1)=maxRSSI;
                  scanners{scannerIdx}.contactCounter(bidx,1)=contactCounter;
                  bidx=bidx+1;
              else
                  unique_filtered=unique([unique_filtered;{beaconID}]);
              endif
          endwhile
          if text_verbosity>=VERBOSITY_INFO
            if(mod(reportNo,10)==0)
              percent=reportNo/scanners{scannerIdx}.noOfReports*100;
              printf("\rScanner ID %06x (%d of %d), %.2f percent done...",scanners{scannerIdx}.nodeID,scannerIdx,noOfScanners,percent);
            endif
          endif
          scanners{scannerIdx}.t=scanners{scannerIdx}.t(1:bidx-1,1);
          scanners{scannerIdx}.beaconID=scanners{scannerIdx}.beaconID(1:bidx-1,1);
          scanners{scannerIdx}.lastRSSI=scanners{scannerIdx}.lastRSSI(1:bidx-1,1);
          scanners{scannerIdx}.maxRSSI=scanners{scannerIdx}.maxRSSI(1:bidx-1,1);
          scanners{scannerIdx}.contactCounter=scanners{scannerIdx}.contactCounter(1:bidx-1,1);
      endfor
      if text_verbosity>=VERBOSITY_INFO
        printf("\rScanner ID %06x (%d of %d), %.2f percent done...\n",scanners{scannerIdx}.nodeID,scannerIdx,noOfScanners,100);
      endif
  endfor
  if text_verbosity>=VERBOSITY_INFO
    printf("Contacts extracted.\n");
  endif
  %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
  if text_verbosity>=VERBOSITY_REPORT
    printf("Calculating report.\n");

    beacons_seen_by_the_network=[];
    for scannerIdx=1:size(scanners,1)
        beacons_seen_by_the_network = unique([beacons_seen_by_the_network; scanners{scannerIdx}.beaconID]);
    endfor

    noOfReport=zeros(size(beacons_seen_by_the_network,1),size(scanners,1));
    for beacIdx=1:size(beacons_seen_by_the_network,1)
        for scannerIdx=1:size(scanners,1)
            noOfReport(beacIdx,scannerIdx)=noOfReport(beacIdx,scannerIdx)+sum(strcmp(scanners{scannerIdx}.beaconID,beacons_seen_by_the_network(beacIdx,1)));
        endfor
    endfor
    
    printf('Available beacons (after filter) %d, %d have been filtered out\n',size(beacons_seen_by_the_network,1), size(unique_filtered,1));
    %FIRST LINE
    printf('| Beacon       |');
    for scannerIdx=1:size(scanners,1)
        printf(' NodeID: %06x |',scanners{scannerIdx}.nodeID);
    endfor
    printf(' GRAND TOTAL    |\n');

    %SECOND LINE
    printf('|              |');
    for scannerIdx=1:size(scanners,1)
        printf('%6d contacts |',sum(noOfReport(:,scannerIdx)));
    endfor
    printf('%6d contacts |\n',sum(sum(noOfReport(:,:))));
    %THIRD LINE
    printf('|              |');
    tot=0;
    for scannerIdx=1:size(scanners,1)
        printf(' %6d reports |',scanners{scannerIdx}.noOfReports);
        tot=tot+scanners{scannerIdx}.noOfReports;
    endfor
    printf(' %6d reports |\n',tot);

    %FURTH LINE
    printf('|--------------|');
    for scannerIdx=1:size(scanners,1)
        printf('----------------|');
    endfor
    printf('----------------|\n');

    %FOLLOWING LINES
    for b_idx=1:size(beacons_seen_by_the_network,1)
        printf('| %s |',beacons_seen_by_the_network{b_idx});
        for scannerIdx=1:size(scanners,1)
            printf('          %5d |',noOfReport(b_idx,scannerIdx));
        endfor
        printf('          %5d |',sum(noOfReport(b_idx,:)));
        printf('\n');
    endfor
    printf('\n');
  endif
  %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
  
  if text_verbosity>=VERBOSITY_INFO
    printf("Analyzing contact data. Extracting node status...\n");
  endif
  
  APPROACHING=1;
  STEADY=0;
  MOVING_AWAY=-1;
  WAS_OUT_OF_RANGE=-2;
  UNDETECTED=-Inf;
  
  REPORT_INTERVAL_S=15;
  ENABLE_HIGH_SPEED_MODE=true; #enables a different threshold based also on d_max and not only on d_time.
  HSM_DELTA_RSSI_THRESHOLD=10;
  
  for scannerIdx=1:size(scanners,1)
      beacons = unique(scanners{scannerIdx}.beaconID);
      scanners{scannerIdx}.beaconStatus=UNDETECTED*ones(size(scanners{scannerIdx}.t)); #allocate the variable for storing the status of the beacon
      
      for b_idx = 1:size(beacons,1)
          beaconID=beacons(b_idx);
          idxs_logical=strcmp(scanners{scannerIdx}.beaconID,beaconID);
          idxs_list=find(idxs_logical);
          t = scanners{scannerIdx}.t(idxs_logical);
          lastRSSI = scanners{scannerIdx}.lastRSSI(idxs_logical);
          maxRSSI = scanners{scannerIdx}.maxRSSI(idxs_logical);
          contactCounter = scanners{scannerIdx}.contactCounter(idxs_logical);
          
          W_LEN=3;  #analysis window length. Unit is number of reports (i.e. the report interval)
          W_INC=1;  #analysis window increment. Unit is number of reports (i.e. the report interval)
          
          if size(idxs_list,1)>=W_LEN
              scanners{scannerIdx}.beaconStatus(1,1)=WAS_OUT_OF_RANGE;
              scanners{scannerIdx}.beaconStatus(2,1)=WAS_OUT_OF_RANGE;
              if ENABLE_HIGH_SPEED_MODE && diff(maxRSSI(1:2,1))>HSM_DELTA_RSSI_THRESHOLD
                scanners{scannerIdx}.beaconStatus(idxs_list(2,1))=APPROACHING;
              endif
            
              for w=1:W_INC:size(t,1)-W_LEN+1
                idxs_list_w=idxs_list(w:w+W_LEN-1);
                time_w_s=t(w:w+W_LEN-1)*24*60*60;
                lastRSSI_w=lastRSSI(w:w+W_LEN-1);
                maxRSSI_w=maxRSSI(w:w+W_LEN-1);
                contactCounter_w=contactCounter(w:w+W_LEN-1);
                
                d_time=diff(time_w_s);
                d_max=diff(maxRSSI_w);
                           
                if max(d_time)<REPORT_INTERVAL_S*2.5 || (ENABLE_HIGH_SPEED_MODE && (d_max(end,1)>HSM_DELTA_RSSI_THRESHOLD || d_max(end,1)<-HSM_DELTA_RSSI_THRESHOLD))
                  #the window is valid
                  if size(find(d_max>0),1)==size(d_max,1) || (ENABLE_HIGH_SPEED_MODE && d_max(end,1)>HSM_DELTA_RSSI_THRESHOLD) #all d_max samples are above 0 or the differential of max rssi is above 10dB
                    #the beacon is getting closer to the scanner
                    scanners{scannerIdx}.beaconStatus(idxs_list_w(end))=APPROACHING;
                  elseif size(find(d_max<0),1)==size(d_max,1) || (ENABLE_HIGH_SPEED_MODE && d_max(end,1)<-HSM_DELTA_RSSI_THRESHOLD)#all d_max samples are below 0 or the differential of max rssi is below -10dB
                    #the beacon is moving away from the scanner
                    scanners{scannerIdx}.beaconStatus(idxs_list_w(end))=MOVING_AWAY;
                  else #some d_max samples are above 0 some below
                    #probably the beacon is roaming around the scanner
                    scanners{scannerIdx}.beaconStatus(idxs_list_w(end))=STEADY;
                  endif
                else
                  #the window is not valid
                  scanners{scannerIdx}.beaconStatus(idxs_list_w(end))=WAS_OUT_OF_RANGE;
                endif
              endfor
          else
            # in this case there are few reports containing this beacon.
            # maybe it is the case of pushing the contatc information anyway?
          endif
          if text_verbosity>=VERBOSITY_INFO
            printf("\rScanner ID %06x (%d of %d), beaconID %s (%d of %d)...", scanners{scannerIdx}.nodeID, scannerIdx, size(scanners,1), beaconID{1}, b_idx, size(beacons,1));
          endif
      endfor
      if text_verbosity>=VERBOSITY_INFO
        printf("done\n");
      endif
  endfor
  if text_verbosity>=VERBOSITY_INFO
    printf("Node status extracted.\n");
  endif
  %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

  if text_verbosity>=VERBOSITY_INFO
    printf("Analyzing node status. Detecting proximity events...\n");
  endif
  
  DETECTOR_IDLE=0;
  DETECTOR_DETECTING=1;
  DETECTOR_DETECTED=2;

  proximity_events=cell(size(scanners,1),1);
  
  for scannerIdx=1:size(scanners,1)
      beacons = unique(scanners{scannerIdx}.beaconID);
      scanners{scannerIdx}.proximity_events=[];
      proximity_events{scannerIdx}={};
      
      
      for b_idx = 1:size(beacons,1)
          beaconID=beacons(b_idx);
          idxs_logical=strcmp(scanners{scannerIdx}.beaconID,beaconID);
          idxs_list=find(idxs_logical);
          t = scanners{scannerIdx}.t(idxs_logical);
          lastRSSI = scanners{scannerIdx}.lastRSSI(idxs_logical);
          maxRSSI = scanners{scannerIdx}.maxRSSI(idxs_logical);
          contactCounter = scanners{scannerIdx}.contactCounter(idxs_logical);
          %proximity detection algorithm          
          scanners{scannerIdx}.proximity_events(b_idx).beaconID=beaconID;
          scanners{scannerIdx}.proximity_events(b_idx).t=[];
          scanners{scannerIdx}.proximity_events(b_idx).t_start=[];
          scanners{scannerIdx}.proximity_events(b_idx).t_end=[];
          scanners{scannerIdx}.proximity_events(b_idx).rssi=[];

          detector_state=DETECTOR_IDLE;
          detector_next_state=DETECTOR_IDLE;
          p_idx_beacon=1;
          evt_rssi=-Inf;
          evt_source=scanners{scannerIdx}.nodeID;
          evt_source_full=scanners{scannerIdx}.nodeID_full;
          evt_beacon=beaconID{1};
          
          while p_idx_beacon<=size(t,1)
            b_status=scanners{scannerIdx}.beaconStatus(idxs_list(p_idx_beacon));
            increment=true;
            if p_idx_beacon==size(t,1) #this is to properly close the algorithm at the end of the file
              b_status=WAS_OUT_OF_RANGE;
            endif
            switch detector_state
              
              case DETECTOR_IDLE
                switch b_status
                  case APPROACHING
                    detector_next_state=DETECTOR_DETECTING;
                    evt_start_t=t(p_idx_beacon);
                    if maxRSSI(p_idx_beacon)>=evt_rssi
                      evt_rssi=maxRSSI(p_idx_beacon);
                      evt_t=t(p_idx_beacon);
                    endif
                  case STEADY
                    detector_next_state=DETECTOR_IDLE;
                  case MOVING_AWAY
                    detector_next_state=DETECTOR_IDLE;
                  case WAS_OUT_OF_RANGE
                    detector_next_state=DETECTOR_IDLE;
                  case UNDETECTED
                    detector_next_state=DETECTOR_IDLE;
                  otherwise
                    warning("unexpected beacon status (b_status=%d) during proximity detection, resetting the FSM",b_status);
                    detector_next_state=DETECTOR_IDLE;
                endswitch
                
              case DETECTOR_DETECTING
                if maxRSSI(p_idx_beacon)>=evt_rssi
                    evt_rssi=maxRSSI(p_idx_beacon);
                    evt_t=t(p_idx_beacon);
                endif
                switch b_status
                  case APPROACHING
                    detector_next_state=DETECTOR_DETECTING;
                  case STEADY
                    detector_next_state=DETECTOR_DETECTING;
                  case MOVING_AWAY
                    detector_next_state=DETECTOR_DETECTED;
                    increment=false; #the FSM needs to do an update without incrementing the index
                  case WAS_OUT_OF_RANGE
                    detector_next_state=DETECTOR_DETECTED;
                    increment=false; #the FSM needs to do an update without incrementing the index
                  case UNDETECTED
                    detector_next_state=DETECTOR_DETECTED;
                    increment=false; #the FSM needs to do an update without incrementing the index
                  otherwise
                    warning("unexpected beacon status (b_status) during proximity detection, resetting the FSM");
                    detector_next_state=DETECTOR_IDLE;
                endswitch
                
              case DETECTOR_DETECTED
                detector_next_state=DETECTOR_IDLE;
                
                #GENERATE EVENT
                if b_status==WAS_OUT_OF_RANGE #this is to avoid creating events that last very long time because b_status jumped from STEADY to WAS_OUT_OF_RANGE without passing from MOVING_AWAY
                  evt_end_t=t(p_idx_beacon-1); #here there is a potential out-of-bound error, however it is impossible, from the FSM design, to reach DETECTOR_DETECTED with p_idx_beacon=1
                else
                  evt_end_t=t(p_idx_beacon);
                endif
                
                #Structure used for internal use (plots,prints) is separated from the structure used as output which is the json
                #scanners{scannerIdx} is for internal use
                #proximity_events{scannerIdx} is for output
                scanners{scannerIdx}.proximity_events(b_idx).t(end+1)=evt_t;
                scanners{scannerIdx}.proximity_events(b_idx).rssi(end+1)=evt_rssi;
                scanners{scannerIdx}.proximity_events(b_idx).t_start(end+1)=evt_start_t;
                scanners{scannerIdx}.proximity_events(b_idx).t_end(end+1)=evt_end_t;
                
                #create and store proximity events                 
                opt.Compact=1;                
                evt={};
                evt_values={};
                
                evt_data.t_start=int64(evt_start_t*24*60*60*1000);
                evt_data.t_end=int64(evt_end_t*24*60*60*1000);
                evt_data.rssi=evt_rssi;
                beaconKeyString=strcat("BID_",evt_beacon);
                evt_ts=int64(evt_t*24*60*60*1000);
                
                use_real_timestamps=true;
                if use_real_timestamps
                  evt_data_string=savejson("",evt_data,opt);
                  evt_values.(beaconKeyString)=evt_data_string;
                  evt.ts=evt_ts;
                  evt.values=evt_values;
                else
                  evt_data.t=evt_ts;
                  evt_data_string=savejson("",evt_data,opt);
                  evt.(beaconKeyString)=evt_data_string;
                endif
                
                %append the event to the global list of events
                proximity_events{scannerIdx}{end+1}=evt;
                
                #reset event variables
                evt_rssi=-Inf;
                evt_t=0;
                  
              otherwise
                warning("unexpected detector FSM state (detector_state) during proximity detection, resetting the FSM");
                detector_next_state=DETECTOR_IDLE;
            endswitch
            detector_state=detector_next_state;
            if increment
              p_idx_beacon++;
            endif
          endwhile
      endfor
  endfor
  
  if text_verbosity>=VERBOSITY_INFO
    printf("Proximity events extracted.\n");
  endif
  
  %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

  if(ENABLE_PLOT)
      if text_verbosity>=VERBOSITY_INFO
        printf("\nPlots start at: %s\n\n",datestr(zero_t))
      endif
      
      ax=zeros(size(scanners,1),1);
      xLinkedAxis=[];
      yLinkedAxis=[];
      
      for scannerIdx=1:size(scanners,1)
          beacons = unique(scanners{scannerIdx}.beaconID);
          
          figure(scannerIdx);
          hold off;
          
          for b_idx = 1:size(beacons,1)
              beaconID=beacons(b_idx);
              idxs_logical=strcmp(scanners{scannerIdx}.beaconID,beaconID);
              idxs_list=find(idxs_logical);
              t = scanners{scannerIdx}.t(idxs_logical)-zero_t;
              lastRSSI = scanners{scannerIdx}.lastRSSI(idxs_logical);
              maxRSSI = scanners{scannerIdx}.maxRSSI(idxs_logical);
              contactCounter = scanners{scannerIdx}.contactCounter(idxs_logical);
              beaconStatus=scanners{scannerIdx}.beaconStatus(idxs_logical);
             
              [axis, L1, L2]=plotyy(t*24*60,lastRSSI,t*24*60,beaconStatus);
              set (L1, "marker", ".");
              set (L1, "linestyle", "--");
              set (L2, "linestyle", "--");
              set (L2, "marker", "o");
              set (L2, "color", "green");
              xLinkedAxis(end+1:end+2,1)=axis;
              yLinkedAxis(end+1,1)=axis(1,1);
              hold on;
              grid on;
              
              [axis, L1, L2] = plotyy(t*24*60,maxRSSI,t*24*60,contactCounter);
              set (L1, "marker", "o");
              set (L2, "linestyle", "--");
              set (L2, "marker", "o");
              set (L2, "color", "yellow");
              xLinkedAxis(end+1:end+2,1)=axis;
              yLinkedAxis(end+1,1)=axis(1,1);
              
              hold on;
              
              xlabel("Time [min]")
              ylabel(axis(2),"Count #");
              ylabel(axis(1),"RSSI [dBm]");
              legend("Last RSSI","MAX RSSI","Status","# Contacs")
            
              if exist('min_time','var')
                  min_time=min([min_time;t]);
                  max_time=max([max_time;t]);
                  min_rssi=min([min_rssi;lastRSSI; maxRSSI]);
                  max_rssi=max([max_rssi;lastRSSI; maxRSSI]);
              else
                  min_time=min(t);
                  max_time=max(t);
                  min_rssi=min([lastRSSI; maxRSSI]);
                  max_rssi=max([lastRSSI; maxRSSI]);
              endif
              
              MIN_RSSI=-100;
              MAX_RSSI=-20;
              ylim([MIN_RSSI MAX_RSSI]);
              
              if max_rssi>MAX_RSSI
                warning("%d samples above RSSI graph limits",sum(lastRSSI>MAX_RSSI)+sum(maxRSSI>MAX_RSSI))
              endif
              if min_rssi<MIN_RSSI
                warning("%d samples under RSSI graph limits",sum(lastRSSI<MIN_RSSI)+sum(maxRSSI<MIN_RSSI))
              endif
                          
              evt_t=scanners{scannerIdx}.proximity_events(b_idx).t;
              evt_rssi=scanners{scannerIdx}.proximity_events(b_idx).rssi;
              evt_start_t=scanners{scannerIdx}.proximity_events(b_idx).t_start;
              evt_end_t=scanners{scannerIdx}.proximity_events(b_idx).t_end;
                          
              for evt_i=1:size(evt_start_t,2)
                duration_line.value=[];
                duration_line.t=[];
              
                #line start point
                duration_line.t(end+1)=evt_start_t(evt_i);
                duration_line.value(end+1)=evt_rssi(evt_i);
                
                #line end point
                duration_line.t(end+1)=evt_end_t(evt_i);
                duration_line.value(end+1)=evt_rssi(evt_i);
                               
                h=line((duration_line.t-zero_t)*24*60,duration_line.value);
                RSSI_EVENT_THRESHOLD=-65;
                
                if duration_line.value>RSSI_EVENT_THRESHOLD
                  set (h, "color", "black");
                else
                  set (h, "color", "cyan");
                endif
                
                #figure(size(scanners,1)+scannerIdx);
                h=plot((evt_t(evt_i)-zero_t)*24*60,evt_rssi(evt_i));
                #grid on;
                #hold on;              
                if evt_rssi(evt_i)>RSSI_EVENT_THRESHOLD
                  set (h, "color", "black");
                else
                  set (h, "color", "cyan");
                endif
                
                set (h, "linestyle", "None");
                set (h, "marker", "o");
                set (h, "markersize", 10);
                xLinkedAxis(end+1,1)=gca();
              endfor
          endfor
          figure(scannerIdx);
          titlestr=sprintf('ScannerID: %06x',scanners{scannerIdx}.nodeID);
          title(titlestr)

      endfor
      
      linkaxes(xLinkedAxis,'x');
      #linkaxes(yLinkedAxis,'y'); #if I link all y axis to have the same RSSI range in all figures, it slinks the x axis just linked
      xlim([min_time max_time]*24*60);
      ylim([-100 max_rssi]);
  %     datetick('x','HH:MM:SS')
  endif

  %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
  % Reorganize the events to comply the format required for thingsboard
  proximity_events_reformat={};
  for scannerIdx=1:size(scanners,1)
    proximity_events_reformat.(scanners{scannerIdx}.nodeID_full)=proximity_events{scannerIdx};
  endfor
  
  
  jsonFilename="json_events.txt";
  proximity_events_json=savejson("proximity_events",proximity_events_reformat,jsonFilename);
  
  if text_verbosity>=VERBOSITY_INFO
    printf("Output is stored in: %s\n",jsonFilename)
  endif
  return
endfunction
