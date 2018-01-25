% Read VelaLab local log file and parse information into data strctures

clear;
close all;
clc;

% packet fields 
dayIDX = 1;
hourIDX = 2;
timestampIDX = 3;
nodeIDX = 4;
nodeIDIDX = 5;
lastIDX = 7;
counterIDX = 9;
dataLenIDX = 11;
payloadStartIDX = 14;


name = '20171215_144106_data';

inputfile = [name '.log'];
outputfile = [name '.mat'];

fileText = fileread(inputfile);
data = textscan(fileText, '%s', 'Delimiter', '\n');
lines = data{1};

j = 1;

for i = 1:size(lines,1)
    longLine = sprintf('%s ', lines{i});
    data = textscan(longLine, '%s', 'Delimiter', ' ');
    words{i,:} = data{1}';
    if strcmp(words{i,1}{1,nodeIDX},'NodeID') == 1
        pktHeaders(j,:) = words{i,1}(1:payloadStartIDX-1);
        pktPayloads{j,1} = words{i,1}(1,payloadStartIDX:end);
        j = j +1;
    end
end

nodeIDS = unique(pktHeaders(:,nodeIDIDX));

for i = 1:size(nodeIDS,1)
    nodeData{i}.nodeID = nodeIDS{i};
    k = 1;
    for j = 1:size(pktHeaders,1)
        if pktHeaders{j,nodeIDIDX} == nodeIDS{i,1}
            
            nodeData{i}.pktHeaders(k,1) = str2double(pktHeaders{j,timestampIDX});
            nodeData{i}.pktHeaders(k,2) = str2double(pktHeaders{j,lastIDX});
            nodeData{i}.pktHeaders(k,3) = str2double(pktHeaders{j,counterIDX});
            nodeData{i}.pktHeaders(k,4) = str2double(pktHeaders{j,dataLenIDX});
            
            t = 1;
            for p = 1:(nodeData{i}.pktHeaders(k,4)/9)
                nodeData{i}.pktData{k,p}{1,1} = pktPayloads{j,1}{1,t};
                nodeData{i}.pktData{k,p}{1,2} = nodeData{i}.pktHeaders(k,1);
                nodeData{i}.pktData{k,p}{1,3} = str2double(pktPayloads{j,1}{1,t+1});
                nodeData{i}.pktData{k,p}{1,4} = str2double(pktPayloads{j,1}{1,t+2});
                nodeData{i}.pktData{k,p}{1,5} = str2double(pktPayloads{j,1}{1,t+3});
                t = t + 4;

            end
            
            k = k + 1;
        end
    end
    nodeData{i}.startTime = nodeData{i}.pktHeaders(1,1);
end

for i = 1:size(nodeData,2)
    
    nodeData{i}.dataCell(1,:) = nodeData{i}.pktData(1,:);
    p = 1;
    for j = 2:size(nodeData{i}.pktHeaders,1)

        if nodeData{i}.pktHeaders(j-1,2) == 0 % previous pkt was not last in sequence, add this to it
            
            for k = 1:size(nodeData{i}.pktData(j,:),2)
                if isempty(nodeData{i}.pktData{j,k})
                    break;
                else
                    for kk = 1:size(nodeData{i}.dataCell(p,:),2)
                        if isempty(nodeData{i}.dataCell{p,kk})
%                             nodeData{i}.dataCell(p,k) = nodeData{i}.pktData(j,k);
%                             nodeData{i}.dataCell{p,k}(1,2) = nodeData{i}.dataCell{j-1,1}(1,2);
                            break;
                        end
                        
                    end
                    
                    if kk < size(nodeData{i}.dataCell(p,:),2)
                        nodeData{i}.dataCell(p,kk) = nodeData{i}.pktData(j,k);
                        nodeData{i}.dataCell{p,kk}(1,2) = nodeData{i}.dataCell{p,1}(1,2);
                    else
                        if isempty(nodeData{i}.dataCell{p,kk})
                            nodeData{i}.dataCell(p,kk) = nodeData{i}.pktData(j,k);
                            nodeData{i}.dataCell{p,kk}(1,2) = nodeData{i}.dataCell{p,1}(1,2);
                        else
                            nodeData{i}.dataCell(p,kk+1) = nodeData{i}.pktData(j,k);
                            nodeData{i}.dataCell{p,kk+1}(1,2) = nodeData{i}.dataCell{p,1}(1,2);
                        end
                    end
                end
            end
            
        else % previous pkt was last ins equence, this is a new one
            
            p = p + 1;
            nodeData{i}.dataCell(p,1:size(nodeData{i}.pktData(j,:),2)) = nodeData{i}.pktData(j,:);

        end
        
    end
    
end




for i = 1:size(nodeData,2)

%     nodeData{i}.data = cell(size(nodeData{i}.dataCell,1),1);
    nodeData{i}.dataAll = {};
    
    for j = 1:size(nodeData{i}.dataCell,1)
        
        for k = 1:size(nodeData{i}.dataCell(j,:),2)
            if isempty(nodeData{i}.dataCell{j,k})
                break;
            else
%                 nodeData{i}.data{j,1} = [nodeData{i}.data{j,1}; nodeData{i}.dataCell{j,k}];
                
                nodeData{i}.dataAll = [nodeData{i}.dataAll; nodeData{i}.dataCell{j,k}];
            end
        end
    end
end



for i = 1:size(nodeData,2)
    
    beaconIDS = unique(nodeData{i}.dataAll(:,1));
    
    for j = 1:size(beaconIDS,1)
        nodeData{i}.contactData{j}.beaconID = beaconIDS{j};
        t = 1;
        
        for k = 1:size(nodeData{i}.dataAll,1)
            
            if nodeData{i}.dataAll{k,1} == beaconIDS{j}
                
                nodeData{i}.contactData{j}.timestamp(t,1) = nodeData{i}.dataAll{k,2};
                nodeData{i}.contactData{j}.lastRSSI(t,1) = nodeData{i}.dataAll{k,3};
                nodeData{i}.contactData{j}.maxRSSI(t,1) = nodeData{i}.dataAll{k,4};
                nodeData{i}.contactData{j}.pktCount(t,1) = nodeData{i}.dataAll{k,5};
                t = t + 1;
                
            end
        end
    end
    
end


save(outputfile, 'nodeData');


