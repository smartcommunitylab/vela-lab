%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
clear
close all

filename="vela-09082019/20190809-141430-UART.log";

%% Open the text file.
fileID = fopen(filename,'r');

text_preallocation_size=1000000;
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

read_only_this_amount_of_lines=-1; %set this to -1 to read the whole file
if read_only_this_amount_of_lines==-1
  printf("Importing file...\n");
else
  printf("Importing the first %d lines from the file...\n",read_only_this_amount_of_lines);
end
formatSpec = "%s";
dataArray = textscan(fileID, formatSpec, read_only_this_amount_of_lines,"Whitespace","","Delimiter","","EndOfLine",'\n');
fclose(fileID);
printf("File imported.\n\n");

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

printf("Parsing packets...\n");
log_date=cell(text_preallocation_size,1);
log_time=cell(text_preallocation_size,1);
log_level=cell(text_preallocation_size,1);
log_origin=cell(text_preallocation_size,1);
log_data=cell(text_preallocation_size,1);
valid_line=1;
lines_to_scan=size(dataArray{1},1);
if lines_to_scan > text_preallocation_size
  warning("Preallocation size isn't enough. To improve performance increase text_preallocation_size to: %d",lines_to_scan)
end
for lineNo=1:lines_to_scan
  #lineNo
  line=dataArray{1}(lineNo,1);
  
  [val, count, errmsg, pos] = sscanf(line{1},"%d/%d/%d %d:%d:%d %s %s %s");
  YYYY=val(1,1);
  MM=val(2,1);
  DD=val(3,1);
  
  hh=val(4,1);
  mm=val(5,1);
  ss=val(6,1);
  
  data=char(val(7:end,1))';
  
  parts=strsplit(data,{'[',']'});
  #segmentare basandosi su [] e tenere quello che c'� dentro come source
  #prendere quello che c'� prima come debug level
  #segmentare basandosi su '' e tenere quello che c'� dentro come data
  
  if(size(parts,2)>2)
    level=parts{1,1};
    origin=parts{1,2};
    text=parts{1,3};
    
    parts_=strsplit(text,'\');
    if(size(parts_,2)>2)
      hex_str=parts_{2}(2:end);
      
      log_date{valid_line,1}=[YYYY, MM, DD];
      log_time{valid_line,1}=[hh,mm,ss];
      log_level{valid_line,1}=level;
      log_origin{valid_line,1}=strcat('[',origin,']');
      log_data{valid_line,1}=hex_str;
      
      valid_line=valid_line+1;
    end
  end
  
  if(mod(lineNo,100)==0)
    percent=lineNo/lines_to_scan*100;
    printf("\r%.2f percent parsed...",percent);
  end
end
log_date=log_date(1:valid_line-1,1);
log_time=log_time(1:valid_line-1,1);
log_level=log_level(1:valid_line-1,1);
log_origin=log_origin(1:valid_line-1,1);
log_data=log_data(1:valid_line-1,1);
printf("\nPackets parsed.\n\n");

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

printf("Analyzing packets.\n");
EXPECTED_AMOUNT_OF_NODES=4;
GATEWAY_ID = 257;

packet_preallocation_size=100000;

scanners=cell(EXPECTED_AMOUNT_OF_NODES,1);
nodesIdxes=[];  %this is used to ease the search of the index, otherwise it should iterate over scanners.scannerIdx
packets_to_analyze=size(log_data,1);
nodeID_len=32;
pkt_type_len=4;
pkt_num_len=2;
for lineNo=1:packets_to_analyze
    if strcmp(log_origin{lineNo},'[SINK]')
        if size(log_data{lineNo,1},2)>=nodeID_len+pkt_type_len+pkt_num_len
            usefull_part = log_data{lineNo}(3:end);
            
            offset=1;
            
            nodeID_ = usefull_part(1,offset:offset+nodeID_len-1);
            nodeID_trunksize=2;
            nodeID=hexstr2int(nodeID_(1,end-nodeID_trunksize+1:end));
            offset=offset+nodeID_len;
            
            pkt_type_=usefull_part(1,offset:offset+pkt_type_len-1);
            pkt_type=hexstr2int(pkt_type_);
            offset=offset+pkt_type_len;
            
            pkt_num_=usefull_part(1,offset:offset+pkt_num_len-1);
            pkt_num=hexstr2int(pkt_num_);
            offset=offset+pkt_num_len;
            
            payload=usefull_part(1,offset:end);
            offset=offset+nodeID_len+1;
            
            %packetHandler(nodeID,pkt_type,pkt_num,payload);
            scannerIdx=find(nodesIdxes==nodeID);
            if isempty(scannerIdx)
                scannerIdx=size(nodesIdxes,1)+1;
                scanners{scannerIdx,1}.nodeID=nodeID;
                scanners{scannerIdx,1}.packets=cell(packet_preallocation_size,1);
                nodesIdxes(scannerIdx,1)=nodeID;
                scanners{scannerIdx,1}.noOfPackets=0;
            end
            packetIdx=scanners{scannerIdx,1}.noOfPackets+1;
            if packetIdx == packet_preallocation_size + 1
              warning("Preallocation size isn't enough. To improve performance increase packet_preallocation_size")
            end
            scanners{scannerIdx,1}.noOfPackets=packetIdx;
            scanners{scannerIdx,1}.packets{packetIdx,1}.pkt_type=pkt_type;
            scanners{scannerIdx,1}.packets{packetIdx,1}.pkt_num=pkt_num;
            scanners{scannerIdx,1}.packets{packetIdx,1}.payload=payload;
            scanners{scannerIdx,1}.packets{packetIdx,1}.timestamp=datenum(log_date{lineNo,1}(1), log_date{lineNo,1}(2), log_date{lineNo,1}(3), log_time{lineNo,1}(1), log_time{lineNo,1}(2), log_time{lineNo,1}(3));
        else
            warning('Short line found: %s',log_data{lineNo});
        end
    else
        nodeID = GATEWAY_ID;
        
        warning('Gateway generated packets (user inputs), are not implemented for now');
    end
    
    if(mod(lineNo,100)==0)
      percent=lineNo/packets_to_analyze*100;
      printf("\r%.2f percent analyzed...",percent);
    end
end
scanners = scanners( 1:size(nodesIdxes,1) ,1 );
for scannerIdx=1:size(nodesIdxes,1)
   scanners{scannerIdx,1}.packets=scanners{scannerIdx,1}.packets(1:scanners{scannerIdx,1}.noOfPackets,1);
end
printf("\nPackets analyzed.\n\n")

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

% PACKET NUM CHECK
MAX_PACKET_COUNT_VALUE=255;
duplicates=zeros(size(scanners));
problems=zeros(size(scanners));
correct=zeros(size(scanners));

printf("Plotting...\n\n")
count_f=figure;
diff_f=figure;
ax_c=zeros(size(scanners,1),1);
ax_f=zeros(size(scanners,1),1);
zero_t=scanners{1,1}.packets{1,1}.timestamp;
for scannerIdx=1:size(scanners,1)
    initialized=false;
    last_valid=1000;
    overflow_offset=0;
       
    num=zeros(size(scanners{scannerIdx,1}.packets,1),1);
    t=zeros(size(scanners{scannerIdx,1}.packets,1),1);
    
    for packetIdx=1:size(scanners{scannerIdx}.packets,1) 
        actual = scanners{scannerIdx,1}.packets{packetIdx,1}.pkt_num;
        num(packetIdx,1)=actual; #the variable num is used for plots only
        t(packetIdx,1)=(scanners{scannerIdx,1}.packets{packetIdx,1}.timestamp-zero_t)*24;
        if initialized            
            if actual == expected %correct counter
                correct(scannerIdx,1)=correct(scannerIdx,1)+1;
            else
                if actual == last %duplicate packet
                    duplicates(scannerIdx,1)=duplicates(scannerIdx,1)+1;
                else  %missing packets or reboots
                    problems(scannerIdx,1)=problems(scannerIdx,1)+1;
                end
            end
        else %first packet (skip any verification)
            correct(scannerIdx,1)=0;
            initialized=true;
        end
        
        next_expected=mod(actual+1,MAX_PACKET_COUNT_VALUE+1);
       
        expected=next_expected; %just to change name to be coerent with the actual meaning of the variable at the next loop
        last=actual;
    end
    
    
    titleStr=sprintf('Node ID: %d',scanners{scannerIdx,1}.nodeID);

    figure(count_f);
    ax_c(scannerIdx,1)=subplot(size(scanners,1),1,scannerIdx);
    plot(t,num,'.')
    title(titleStr);
    grid on
    box on
    
    figure(diff_f);
    ax_f(scannerIdx,1)=subplot(size(scanners,1),1,scannerIdx);
    valid_idx=[false; diff(num)~=-MAX_PACKET_COUNT_VALUE];
    pkt_num_diff = diff(num);
    figure(diff_f);
    plot(t(valid_idx),pkt_num_diff(valid_idx(2:end)),'o')
    title(titleStr);
    grid on
    box on

end
linkaxes(ax_c,'x')
linkaxes(ax_f,'x')
% PRINT OUTPUT
printf('| NodeID |  #Packets | #Duplicates | #Problems |\n')
for scannerIdx=1:size(scanners,1)
    printf('|   %3d  |   %6d  |   %6d    |  %6d   |\n',scanners{scannerIdx,1}.nodeID,size(scanners{scannerIdx}.packets,1),duplicates(scannerIdx,1),problems(scannerIdx,1));
end

printf("Plotting battery...\n\n")

ax=zeros(6,1);
zero_t=scanners{1,1}.packets{1,1}.timestamp;
for scannerIdx=1:size(scanners,1)
         
    b_cap_mAh=zeros(size(scanners{scannerIdx}.packets,1),1);
    b_soc=zeros(size(scanners{scannerIdx}.packets,1),1);
    b_tte_min=zeros(size(scanners{scannerIdx}.packets,1),1);
    b_cons_100uA=zeros(size(scanners{scannerIdx}.packets,1),1);
    b_volt_mV=zeros(size(scanners{scannerIdx}.packets,1),1);
    b_temp_10mdeg=zeros(size(scanners{scannerIdx}.packets,1),1);
    t=zeros(size(scanners{scannerIdx}.packets,1),1);
    actual_idx=1;
    
    for packetIdx=1:size(scanners{scannerIdx}.packets,1)
        pkt_type_hex_string=sprintf("%04X",scanners{scannerIdx,1}.packets{packetIdx,1}.pkt_type);
        if strcmp(pkt_type_hex_string,"0200")
          t(actual_idx,1)=(scanners{scannerIdx,1}.packets{packetIdx,1}.timestamp-zero_t)*24;

          b_cap_raw=scanners{scannerIdx,1}.packets{packetIdx,1}.payload(1:4);
          b_cap_mAh(actual_idx,1)=sscanf(b_cap_raw,"%x");
          b_soc_raw=scanners{scannerIdx,1}.packets{packetIdx,1}.payload(5:8);
          b_soc(actual_idx,1)=sscanf(b_soc_raw,"%x");
          b_tte_raw=scanners{scannerIdx,1}.packets{packetIdx,1}.payload(9:12);
          b_tte_min(actual_idx,1)=sscanf(b_tte_raw,"%x");
          b_cons_raw=scanners{scannerIdx,1}.packets{packetIdx,1}.payload(13:16);
          b_cons_100uA(actual_idx,1)=typecast(uint16(sscanf(b_cons_raw,'%x')), 'int16');
          b_volt_raw=scanners{scannerIdx,1}.packets{packetIdx,1}.payload(17:20);
          b_volt_mV(actual_idx,1)=sscanf(b_volt_raw,"%x");
          b_temp_raw=scanners{scannerIdx,1}.packets{packetIdx,1}.payload(21:24);
          b_temp_10mdeg(actual_idx,1)=typecast(uint16(sscanf(b_temp_raw,'%x')), 'int16');
          
          actual_idx=actual_idx+1;
        end
    end
    
    if actual_idx > 1
      b_cap_mAh=b_cap_mAh(1:actual_idx-1);
      b_soc=b_soc(1:actual_idx-1);
      b_tte_min=b_tte_min(1:actual_idx-1);
      b_cons_100uA=b_cons_100uA(1:actual_idx-1);
      b_volt_mV=b_volt_mV(1:actual_idx-1);
      b_temp_10mdeg=b_temp_10mdeg(1:actual_idx-1);
      t=t(1:actual_idx-1);
      
      figure;    
      
      ax(1,1)=subplot(6,1,1);
      plot(t,b_cap_mAh,'o-')
      grid on
      box on
      ylabel("Capacity mAh");
      titlestr=sprintf('ScannerID: %d',scanners{scannerIdx,1}.nodeID);
      title(titlestr)
      
      ax(2,1)=subplot(6,1,2);
      plot(t,b_soc/10,'o-')
      grid on
      box on
      ylabel("State of charge %");
      
      ax(3,1)=subplot(6,1,3);
      plot(t,b_tte_min/60,'o-')
      grid on
      box on
      ylabel("Time to live [h]");
      
      ax(4,1)=subplot(6,1,4);
      plot(t,b_cons_100uA/10,'o-')
      grid on
      box on
      ylabel("Current conumption [mA]");
      
      ax(5,1)=subplot(6,1,5);
      plot(t,b_volt_mV/1000,'o-')
      grid on
      box on
      ylabel("battery voltage [V]");
      axis([t(1,1), t(end,1), 3,max(b_volt_mV/1000)])
      
      ax(6,1)=subplot(6,1,6);
      plot(t,b_temp_10mdeg/100,'o-')
      grid on
      box on
      ylabel("battery temperature [�C]");
      
      linkaxes(ax,'x')
    else
      printf('ScannerID %d does not have any battery information.',scanners{scannerIdx,1}.nodeID)
    end
end

save last_packet_analyzer_run.mat