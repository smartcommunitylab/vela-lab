clear
close all;

% set ID_TO_ANALYZE to empty to remove the filter
ID_TO_ANALYZE={
 %  '0000FF0000F2';
 %  '0000FF0000F3';
 %  '0000FF0000F4';
 %  '0000FF0000F5';
 %  '0000FF0000F6';
    };

ID_TO_REMOVE={
%   '0000FF0000F2';
   '0000FF0000F3';
   '0000FF0000F4';
   '0000FF0000F5';
   '0000FF0000F6';
   '00000000004F';
   '000000000050';
   '000000000051';
   '000000000052';
   '000000000053';
   '000000000054';
   '000000000055';
   '000000000056';
   '000000000057';
   '000000000058';
   '000000000059';
   '00000000005A';
   '00000000005B';
   '00000000005C';
   '00000000005D';
};

ENABLE_PLOT=true;
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

filename="vela-09082019/20190809-141430-contact.log";

%% Open the text file.
fileID = fopen(filename,'r');
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

printf("Importing file...\n");
formatSpec = "%s";
dataArray = textscan(fileID, formatSpec, -1,"Whitespace","","Delimiter","","EndOfLine",'\n');
fclose(fileID);
printf("File imported.\n\n");

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
text_preallocation_size=100000;
printf("Parsing packets...\n");
log_date=zeros(text_preallocation_size,3);
log_time=zeros(text_preallocation_size,3);
log_origin=zeros(text_preallocation_size,1);
log_data=cell(text_preallocation_size,1);
valid_line=1;
lines_to_scan=size(dataArray{1},1);
if lines_to_scan > text_preallocation_size
  warning("Preallocation size isn't enough. To improve performance increase text_preallocation_size to: %d",lines_to_scan)
end

for lineNo=1:lines_to_scan
  #lineNo
  line=dataArray{1}(lineNo,1);
  
  [val, count, errmsg, pos] = sscanf(line{1},"%d-%d-%d %d:%d:%d Node %s %s");
  YYYY=val(1,1);
  MM=val(2,1);
  DD=val(3,1);
  
  hh=val(4,1);
  mm=val(5,1);
  ss=val(6,1);
  
  data=char(val(7:end,1))';
  
  nodeID_len=32;
  nodeID_ = data(1,1:nodeID_len);
  nodeID_trunksize=2;
  nodeID=hexstr2int(nodeID_(1,end-nodeID_trunksize+1:end));
  
  origin=nodeID;
  hex_str=data;
  
  log_date(valid_line,:)=[YYYY, MM, DD];
  log_time(valid_line,:)=[hh,mm,ss];
  log_origin(valid_line,1)=origin;
  log_data{valid_line,1}=data(1,nodeID_len+1:end);
  
  valid_line=valid_line+1;
  
  if(mod(lineNo,100)==0)
    percent=lineNo/lines_to_scan*100;
    printf("\r%.2f percent parsed...",percent);
  end
end
log_date=log_date(1:valid_line-1,:);
log_time=log_time(1:valid_line-1,:);
log_data=log_data(1:valid_line-1,:);
printf("\nPackets parsed.\n\n");

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

printf("Pre analisys...\n");
EXPECTED_AMOUNT_OF_NODES=4;

packet_preallocation_size=100000;

#scanners=cell(EXPECTED_AMOUNT_OF_NODES,1);
nodesIdxes=[];  %this is used to ease the search of the index, otherwise it should iterate over scanners.scannerIdx
packets_to_analyze=size(log_data,1);
for lineNo=1:packets_to_analyze
  if size(log_data{lineNo,1},2)>14
      usefull_part = log_data{lineNo};
      
      offset=1;
      
      nodeID_len=32;
      nodeID=log_origin(lineNo);
      offset=offset+nodeID_len;
      
      pkt_type_len=4;
      offset=offset+pkt_type_len;
      
      pkt_num_len=2;
      offset=offset+pkt_num_len;
      
      contactsData=usefull_part(1,offset:end);
      offset=offset+nodeID_len;
      
      scannerIdx=find(nodesIdxes==nodeID);
      if isempty(scannerIdx)
          scannerIdx=size(nodesIdxes,1)+1;
          scanners{scannerIdx,1}.nodeID=nodeID;
          scanners{scannerIdx,1}.noOfReports=0;
          nodesIdxes(scannerIdx,1)=nodeID;
          scanners{scannerIdx,1}.noOfContacts=0;
      end

      scanners{scannerIdx,1}.noOfReports=scanners{scannerIdx,1}.noOfReports+1;
      cur = 1;
      while cur < size(contactsData,2)
          beaconID=contactsData(cur:cur+11);
          cur=cur+18;

          isBeaconValid = (isempty(ID_TO_ANALYZE) || sum(strcmp(ID_TO_ANALYZE,beaconID))) && (isempty(ID_TO_REMOVE) || ~sum(strcmp(ID_TO_REMOVE,beaconID)) );
          if isBeaconValid
              scanners{scannerIdx}.noOfContacts=scanners{scannerIdx}.noOfContacts+1;
          end
      end
      
  else
      warning('Short line found: %s',log_data{lineNo});
  end
    
  if(mod(lineNo,100)==0)
    percent=lineNo/packets_to_analyze*100;
    printf("\r%.2f percent analyzed...",percent);
  end
end
noOfScanners=size(scanners,1);
printf("Pre analysis done.\n");

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

printf("\nAnalyzing...\n");
unique_filtered=[];
for scannerIdx=1:size(scanners,1)
    selected_idx = log_origin==scanners{scannerIdx,1}.nodeID;
    
    if scannerIdx==1
      zero_t=datenum(log_date(1,1), log_date(1,2), log_date(1,3), log_time(1,1), log_time(1,2), log_time(1,3));
    end 
    timestamps=datenum(log_date(selected_idx,1), log_date(selected_idx,2), log_date(selected_idx,3), log_time(selected_idx,1), log_time(selected_idx,2), log_time(selected_idx,3)) - zero_t;

    contactsData=cell(scanners{scannerIdx,1}.noOfReports,1);

    i=find(selected_idx);
    for c=1:size(i,1)
      contactsData{c,1} = log_data{i(c,1)};
    end
    
    scanners{scannerIdx}.noOfReports = sum(selected_idx); #should be the same number
    
    scanners{scannerIdx}.time=NaN(scanners{scannerIdx}.noOfContacts,1);
    scanners{scannerIdx}.beaconID=cell(scanners{scannerIdx}.noOfContacts,1);
    scanners{scannerIdx}.lastRSSI=zeros(scanners{scannerIdx}.noOfContacts,1);
    scanners{scannerIdx}.maxRSSI=zeros(scanners{scannerIdx}.noOfContacts,1);
    scanners{scannerIdx}.contactCounter=zeros(scanners{scannerIdx}.noOfContacts,1);
    bidx = 1;
    
    for line = 1:scanners{scannerIdx}.noOfReports
    
        cur = 1;
        timestamp=timestamps(line);
        while cur < size(contactsData{line},2)
            beaconID=contactsData{line}(cur:cur+11);
            cur=cur+12;
            
            lastRSSI=typecast(uint8(sscanf(contactsData{line}(cur:cur+1),'%x')), 'int8');
            cur=cur+2;
            maxRSSI=typecast(uint8(sscanf(contactsData{line}(cur:cur+1),'%x')), 'int8');
            cur=cur+2;
            contactCounter=uint8(hexstr2int(contactsData{line}(cur:cur+1)));
            cur=cur+2;

            isBeaconValid = (isempty(ID_TO_ANALYZE) || sum(strcmp(ID_TO_ANALYZE,beaconID))) && (isempty(ID_TO_REMOVE) || ~sum(strcmp(ID_TO_REMOVE,beaconID)) );
            if isBeaconValid
                scanners{scannerIdx}.time(bidx,1)=timestamp;
                scanners{scannerIdx}.beaconID{bidx,1}=beaconID;
                scanners{scannerIdx}.lastRSSI(bidx,1)=lastRSSI;
                scanners{scannerIdx}.maxRSSI(bidx,1)=maxRSSI;
                scanners{scannerIdx}.contactCounter(bidx,1)=contactCounter;
                bidx=bidx+1;
            else
                unique_filtered=unique([unique_filtered;{beaconID}]);
            end
        end
        if(mod(line,10)==0)
          percent=line/scanners{scannerIdx}.noOfReports*100;
          printf("\rScanner ID %d (%d of %d), %.2f percent done...",scanners{scannerIdx}.nodeID,scannerIdx,noOfScanners,percent);
        end
        scanners{scannerIdx}.time=scanners{scannerIdx}.time(1:bidx-1,1);
        scanners{scannerIdx}.beaconID=scanners{scannerIdx}.beaconID(1:bidx-1,1);
        scanners{scannerIdx}.lastRSSI=scanners{scannerIdx}.lastRSSI(1:bidx-1,1);
        scanners{scannerIdx}.maxRSSI=scanners{scannerIdx}.maxRSSI(1:bidx-1,1);
        scanners{scannerIdx}.contactCounter=scanners{scannerIdx}.contactCounter(1:bidx-1,1);
    end
    printf("\rScanner ID %d (%d of %d), %.2f percent done...\n",scanners{scannerIdx}.nodeID,scannerIdx,noOfScanners,100);
end
printf("Analysis done.\n");

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

printf("Calculating report.\n");

beacons_seen_by_the_network=[];
for scannerIdx=1:size(scanners,1)
    beacons_seen_by_the_network = unique([beacons_seen_by_the_network; scanners{scannerIdx}.beaconID]);
end

noOfReport=zeros(size(beacons_seen_by_the_network,1),size(scanners,1));
for beacIdx=1:size(beacons_seen_by_the_network,1)
    for scannerIdx=1:size(scanners,1)
        noOfReport(beacIdx,scannerIdx)=noOfReport(beacIdx,scannerIdx)+sum(strcmp(scanners{scannerIdx}.beaconID,beacons_seen_by_the_network(beacIdx,1)));
    end
end
printf('Available beacons (after filter) %d, %d have been filtered out\n',size(beacons_seen_by_the_network,1), size(unique_filtered,1));

%FIRST LINE
printf('| Beacon       |');
for scannerIdx=1:size(scanners,1)
    printf(' NodeID : %3d   |',scanners{scannerIdx}.nodeID);
end
printf(' GRAND TOTAL    |\n');

%SECOND LINE
printf('|              |');
for scannerIdx=1:size(scanners,1)
    printf('%6d contacts |',sum(noOfReport(:,scannerIdx)));
end
printf('%6d contacts |\n',sum(sum(noOfReport(:,:))));
%THIRD LINE
printf('|              |');
tot=0;
for scannerIdx=1:size(scanners,1)
    printf(' %6d reports |',scanners{scannerIdx}.noOfReports);
    tot=tot+scanners{scannerIdx}.noOfReports;
end
printf(' %6d reports |\n',tot);

%FURTH LINE
printf('|--------------|');
for scannerIdx=1:size(scanners,1)
    printf('----------------|');
end
printf('----------------|\n');

%FOLLOWING LINES
for iii=1:size(beacons_seen_by_the_network,1)
    printf('| %s |',beacons_seen_by_the_network{iii});
    for scannerIdx=1:size(scanners,1)
        printf('          %5d |',noOfReport(iii,scannerIdx));
    end
    printf('          %5d |',sum(noOfReport(iii,:)));
    printf('\n');
end

if(ENABLE_PLOT)
    printf("\nPlots start at: %s\n\n",datestr(zero_t))

    figure;
    ax=zeros(size(scanners,1),1);
    
    for scannerIdx=1:size(scanners,1)
        beacons = unique(scanners{scannerIdx}.beaconID);

        ax(scannerIdx,1)=subplot(size(scanners,1),1,scannerIdx);

        for iii = 1:size(beacons,1)
            beaconID=beacons(iii);
            idxs = strcmp(scanners{scannerIdx}.beaconID,beaconID);
            time = scanners{scannerIdx}.time(idxs)*24*60;
            lastRSSI = scanners{scannerIdx}.lastRSSI(idxs);
            maxRSSI = scanners{scannerIdx}.maxRSSI(idxs);
            contactCounter = scanners{scannerIdx}.contactCounter(idxs);
            plot(time,maxRSSI,'o',time,lastRSSI,'.');
            xlabel("Time [min]")
            ylabel("RSSI [dBm]")
            legend("MAX RSSI","Last RSSI")
            grid on;
            hold on;
            
            if exist('min_time','var')
                min_time=min([min_time;time]);
                max_time=max([max_time;time]);
                min_rssi=min([min_rssi;lastRSSI; maxRSSI]);
                max_rssi=max([max_rssi;lastRSSI; maxRSSI]);
            else
                min_time=min(time);
                max_time=max(time);
                min_rssi=min([lastRSSI; maxRSSI]);
                max_rssi=max([lastRSSI; maxRSSI]);
            end
        end
        titlestr=sprintf('ScannerID: %d',scanners{scannerIdx}.nodeID);
        title(titlestr)
    end
    
    linkaxes(ax,'xy');
    xlim([min_time max_time]);
    ylim([-100 max_rssi]);
%     datetick('x','HH:MM:SS')
end
