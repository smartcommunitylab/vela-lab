% plot maxRSSI for a specified beacon

clear;
close all;
clc;

% % acquisizione 1 - test
% name = '20171214-170015-data';
% 
% % acquisizione 2 - davide arriva da lontano, bojan sta fermo con un nodo (F0)
% name = '20171214-171827-data';
% 
% % acquisizione 3 - davide lontano con 3 nodi, accende, fa il percorso del pedibus con fermata al parco
% name = '20171214-172452-data';
% 
% % acquisizione 4 - davide sotto il 135 con 3 nodi, accende nodi, aspetta un po poi fa il percorso del pedibus
% name = '20171214-173414-data';
% 
% % acquisizione 5 - test con 30 multinodi
% name = '20171214-174352-data';
% 
% % acquisizione 6 - davide sotto il 135 con 30 multinodi, accende nodi, aspetta un po poi fa il percorso del pedibus
% name = '20171214-174534-data';
% 
% % acquisizione 7 - davide sotto il 135 con 45 multinodi, accende nodi, aspetta un po poi fa il percorso del pedibus
% name = '20171214-175936-data';
% 
% % acquisizione 8 - 180451- davide sotto 135 con 3 nodi, accende i nodi, aspetta e popi fa il percorso sbagliato
% name = '20171214-180451-data';

%name = 'VELA_19_12_17\20171219_124954_data';
name = '20171220_123729_data';

inputfile = [name '.mat'];
load(inputfile);

% beacon to plot maxRSSI
myBeaconID{1} = '000000000040';
myBeaconID{2} = '000000000044';
myBeaconID{3} = '00000000004E';
myBeaconID{4} = '000000000050';
myBeaconID{5} = '00000000005D';
myBeaconID{6} = '00000000005E';
myBeaconID{7} = '00000000006C';

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

COUNTERIDX = 3;
COUNTER_OVERFLOW = 127;
DATE_FORMAT = 'HH:MM:SS';
MAX_RSSI_SCALE = -20;
MIN_RSSI_SCALE = -100;

for nodeIDX = 1:size(nodeData,2)
    
    pktCounter{nodeIDX} = nodeData{nodeIDX}.pktHeaders(:,COUNTERIDX);
    
    for i = 2:size(pktCounter{nodeIDX},1)
        
        if pktCounter{nodeIDX}(i,1) < pktCounter{nodeIDX}(i-1,1)
            pktCounter{nodeIDX}(i:end,1) = pktCounter{nodeIDX}(i:end,1)+COUNTER_OVERFLOW;
        end
        
    end

end

pktLost = zeros(1,size(nodeData,2));

for nodeIDX = 1:size(nodeData,2)
    
    for i = 2:size(pktCounter{nodeIDX},1)
        
        gap = pktCounter{nodeIDX}(i,1)-pktCounter{nodeIDX}(i-1,1);
        if gap > 1
            pktLost(nodeIDX) = pktLost(nodeIDX) + gap - 1;
        end
        
    end
    
    PER(nodeIDX) = pktLost(nodeIDX) / (pktLost(nodeIDX) + size(pktCounter{nodeIDX},1)) * 100;
    
end









%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
% beacon index in the data for each node
beaconIDX = zeros(size(myBeaconID,2),size(nodeData,2));
startTimeAll = zeros(1,size(nodeData,2));
endTimeAll = zeros(1,size(nodeData,2));
availableBeaconID = [];
availableNodeID = [];

for nodeIDX = 1:size(nodeData,2)

    for bIDX = 1:size(myBeaconID,2)

        for i = 1:size(nodeData{nodeIDX}.contactData,2)
            availableBeaconID = [availableBeaconID; nodeData{nodeIDX}.contactData{1,i}.beaconID];
            if strcmp(nodeData{nodeIDX}.contactData{1,i}.beaconID, myBeaconID{bIDX}) == 1
                beaconIDX(bIDX,nodeIDX) = i;
            end
        end

        if beaconIDX(bIDX,nodeIDX) ~= 0
            myBeaconData{bIDX,nodeIDX}.timestamp = nodeData{nodeIDX}.contactData{1,beaconIDX(bIDX,nodeIDX)}.timestamp;
            myBeaconData{bIDX,nodeIDX}.maxRSSI = nodeData{nodeIDX}.contactData{1,beaconIDX(bIDX,nodeIDX)}.maxRSSI;
        else
            myBeaconData{bIDX,nodeIDX}.timestamp = [];
            myBeaconData{bIDX,nodeIDX}.maxRSSI = [];

        end
    end
    
    startTimeAll(1,nodeIDX) = nodeData{nodeIDX}.startTime;
    endTimeAll(1,nodeIDX) = nodeData{nodeIDX}.endTime;
    availableNodeID = [availableNodeID sscanf(nodeData{nodeIDX}.nodeID, '%d')];
end

startTime = min(startTimeAll);
endTime = max(endTimeAll);
availableBeaconID = unique(availableBeaconID,'rows');

for nodeIDX = 1:size(nodeData,2)

    for bIDX = 1:size(myBeaconID,2)

    myBeaconData{bIDX,nodeIDX}.timeData = datetime(myBeaconData{bIDX,nodeIDX}.timestamp./1000,'ConvertFrom','posixtime');
    myBeaconData{bIDX,nodeIDX}.timeRel = myBeaconData{bIDX,nodeIDX}.timestamp - startTime;

%     figure;
%     plot(myBeaconData{1,nodeIDX}.timeRel/1000, myBeaconData{1,nodeIDX}.maxRSSI, 'o', 'MarkerEdgeColor','b', 'MarkerFaceColor','b', 'MarkerSize',8);
%     title(['NodeID: ' nodeData{nodeIDX}.nodeID]);
%     xlabel('Time (s)');
%     ylabel('RSSI (dB)');
    end    
end

% figure;
% hold on;
% plot(myBeaconData{1,1}.timeRel/1000, myBeaconData{1,1}.maxRSSI, '--o', 'MarkerEdgeColor','b', 'MarkerFaceColor','b', 'MarkerSize',8);
% plot(myBeaconData{1,2}.timeRel/1000, myBeaconData{1,2}.maxRSSI, '--o', 'MarkerEdgeColor','g', 'MarkerFaceColor','g', 'MarkerSize',8);
% plot(myBeaconData{1,3}.timeRel/1000, myBeaconData{1,3}.maxRSSI, '--o', 'MarkerEdgeColor','r', 'MarkerFaceColor','r', 'MarkerSize',8);
% plot(myBeaconData{1,4}.timeRel/1000, myBeaconData{1,4}.maxRSSI, '--o', 'MarkerEdgeColor','m', 'MarkerFaceColor','m', 'MarkerSize',8);
% title(['BeaconID: ' myBeaconID{1}]);
% xlabel('Time (s)');
% ylabel('RSSI (dB)');
% legend(nodeData{1}.nodeID, nodeData{2}.nodeID, nodeData{3}.nodeID, nodeData{4}.nodeID)

EXPECTED_REPORT_INTERVAL_S = 5;
myNodeID = availableNodeID;%[4 132]; %this changes the order of nodes in the plot. Set this to availableNodeID for the default order
warnRep = false;
for nodeID = myNodeID
    legendStr = [];
    nodeIDX = find(nodeID==availableNodeID);
    if ~isempty(nodeIDX)
        ax2(nodeIDX) = subplot(size(myNodeID,2),1, find(nodeID==myNodeID));
        hold on;
        for beaconNo = 1:size(myBeaconData,1)
            if(min(diff(myBeaconData{beaconNo,nodeIDX}.timeData)) < seconds(EXPECTED_REPORT_INTERVAL_S*0.8))
                if warnRep == false
                    warning(['The EXPECTED_REPORT_INTERVAL_S is set to: ' sprintf('%d S',EXPECTED_REPORT_INTERVAL_S) ' but at least one packet arrived with ' datestr(min(diff(myBeaconData{beaconNo,nodeIDX}.timeData)), 'MM:SS') ' interval from the previous one.']);
                    warnRep = true;
                end
            end
            NaNtimeData = myBeaconData{beaconNo,nodeIDX}.timeData(diff(myBeaconData{beaconNo,nodeIDX}.timeData) > seconds(3.1*EXPECTED_REPORT_INTERVAL_S)) + seconds(EXPECTED_REPORT_INTERVAL_S/10);
            data_plot = [myBeaconData{beaconNo,nodeIDX}.maxRSSI ; NaN*ones(size(NaNtimeData))];
            time_plot = [myBeaconData{beaconNo,nodeIDX}.timeData; NaNtimeData];
            [time_plot, o] = sort(time_plot);
            data_plot = data_plot(o);
            p = plot(time_plot, data_plot, '-o', 'MarkerSize',8, 'LineWidth',2);
            p.MarkerFaceColor = p.Color;
            legendStr = [legendStr; 'Beacon: 0x' myBeaconID{beaconNo}(end-1:end)];
        end
        if(find(nodeIDX == availableNodeID) == 1)
            l = legend(legendStr);
            l.FontSize = 12;
            l.FontWeight = 'bold';
        end
        ax2(nodeIDX).FontSize=12;
        ax2(nodeIDX).FontWeight='bold';
        ax2(nodeIDX).YTick=MIN_RSSI_SCALE:10:MAX_RSSI_SCALE;
        %ax2(nodeID).XTick=datenum(2017,12,19,11,50,30):datenum(seconds(15)):datenum(2017,12,19,11,55,00);
        hold off;
        ylim([MIN_RSSI_SCALE,MAX_RSSI_SCALE]);
        grid on;
        l = ylabel(['NodeID: ' nodeData{nodeIDX}.nodeID sprintf('\n') 'RSSI [dBm]']);
        
        l.FontSize = 12;
        l.FontWeight = 'bold';
    end
end
linkaxes(ax2, 'xy');
