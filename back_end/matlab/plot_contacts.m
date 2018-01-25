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

name = '20171215_144106_data';

inputfile = [name '.mat'];
load(inputfile);

% beacon to plot maxRSSI
% myBeaconID{1} = '0000000000F0';
% myBeaconID{2} = '0000000000F1';
% myBeaconID{3} = '0000000000F3';

myBeaconID{1} = '000000000040';
myBeaconID{2} = '000000000041';
myBeaconID{3} = '000000000043';







%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

COUNTERIDX = 3;
COUNTER_OVERFLOW = 127;

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

for nodeIDX = 1:size(nodeData,2)

    for bIDX = 1:size(myBeaconID,2)

        for i = 1:size(nodeData{nodeIDX}.contactData,2)
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
    
    startTimeAll(1,nodeIDX) = nodeData{1}.startTime;

end

startTime = min(startTimeAll);


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

figure;
hold on;
plot(myBeaconData{1,1}.timeRel/1000, myBeaconData{1,1}.maxRSSI, '--o', 'MarkerEdgeColor','b', 'MarkerFaceColor','b', 'MarkerSize',8);
plot(myBeaconData{1,2}.timeRel/1000, myBeaconData{1,2}.maxRSSI, '--o', 'MarkerEdgeColor','g', 'MarkerFaceColor','g', 'MarkerSize',8);
plot(myBeaconData{1,3}.timeRel/1000, myBeaconData{1,3}.maxRSSI, '--o', 'MarkerEdgeColor','r', 'MarkerFaceColor','r', 'MarkerSize',8);
plot(myBeaconData{1,4}.timeRel/1000, myBeaconData{1,4}.maxRSSI, '--o', 'MarkerEdgeColor','m', 'MarkerFaceColor','m', 'MarkerSize',8);
title(['BeaconID: ' myBeaconID{1}]);
xlabel('Time (s)');
ylabel('RSSI (dB)');
legend(nodeData{1}.nodeID, nodeData{2}.nodeID, nodeData{3}.nodeID, nodeData{4}.nodeID)


figure;
ax(1) = subplot(4,1,1);
plot(myBeaconData{1,1}.timeRel/1000, myBeaconData{1,1}.maxRSSI, 'o', 'MarkerEdgeColor','b', 'MarkerFaceColor','b', 'MarkerSize',8);
ylim([-100,-10]);
title(['BeaconID: ' myBeaconID{1}]);
ylabel(['NodeID: ' nodeData{1}.nodeID]);
ax(2) = subplot(4,1,2);
plot(myBeaconData{1,2}.timeRel/1000, myBeaconData{1,2}.maxRSSI, 'o', 'MarkerEdgeColor','g', 'MarkerFaceColor','g', 'MarkerSize',8);
ylim([-100,-10]);
ylabel(['NodeID: ' nodeData{2}.nodeID]);
ax(3) = subplot(4,1,3);
plot(myBeaconData{1,3}.timeRel/1000, myBeaconData{1,3}.maxRSSI, 'o', 'MarkerEdgeColor','r', 'MarkerFaceColor','r', 'MarkerSize',8);
ylim([-100,-10]);
ylabel(['NodeID: ' nodeData{3}.nodeID]);
ax(4) = subplot(4,1,4);
plot(myBeaconData{1,4}.timeRel/1000, myBeaconData{1,4}.maxRSSI, 'o', 'MarkerEdgeColor','m', 'MarkerFaceColor','m', 'MarkerSize',8);
ylim([-100,-10]);
xlabel('Time (s)');
ylabel(['NodeID: ' nodeData{4}.nodeID]);
% linkaxes(ax, 'x');


figure;
ax2(1) = subplot(4,1,1);
hold on;
plot(myBeaconData{1,1}.timeRel/1000, myBeaconData{1,1}.maxRSSI, 'o', 'MarkerEdgeColor','b', 'MarkerFaceColor','b', 'MarkerSize',8);
plot(myBeaconData{2,1}.timeRel/1000, myBeaconData{2,1}.maxRSSI, 'o', 'MarkerEdgeColor','g', 'MarkerFaceColor','g', 'MarkerSize',8);
plot(myBeaconData{3,1}.timeRel/1000, myBeaconData{3,1}.maxRSSI, 'o', 'MarkerEdgeColor','r', 'MarkerFaceColor','r', 'MarkerSize',8);
hold off;
ylim([-100,-10]);
% xlim([90,350]);
grid on;
ylabel(['NodeID: ' nodeData{1}.nodeID]);
legend(myBeaconID{1}, myBeaconID{2}, myBeaconID{3});
ax2(2) = subplot(4,1,2);
hold on;
plot(myBeaconData{1,2}.timeRel/1000, myBeaconData{1,2}.maxRSSI, 'o', 'MarkerEdgeColor','b', 'MarkerFaceColor','b', 'MarkerSize',8);
plot(myBeaconData{2,2}.timeRel/1000, myBeaconData{2,2}.maxRSSI, 'o', 'MarkerEdgeColor','g', 'MarkerFaceColor','g', 'MarkerSize',8);
plot(myBeaconData{3,2}.timeRel/1000, myBeaconData{3,2}.maxRSSI, 'o', 'MarkerEdgeColor','r', 'MarkerFaceColor','r', 'MarkerSize',8);
hold off;
ylim([-100,-10]);
grid on;
ylabel(['NodeID: ' nodeData{2}.nodeID]);
ax2(3) = subplot(4,1,3);
hold on;
plot(myBeaconData{1,3}.timeRel/1000, myBeaconData{1,3}.maxRSSI, 'o', 'MarkerEdgeColor','b', 'MarkerFaceColor','b', 'MarkerSize',8);
plot(myBeaconData{2,3}.timeRel/1000, myBeaconData{2,3}.maxRSSI, 'o', 'MarkerEdgeColor','g', 'MarkerFaceColor','g', 'MarkerSize',8);
plot(myBeaconData{3,3}.timeRel/1000, myBeaconData{3,3}.maxRSSI, 'o', 'MarkerEdgeColor','r', 'MarkerFaceColor','r', 'MarkerSize',8);
hold off;
ylim([-100,-10]);
grid on;
ylabel(['NodeID: ' nodeData{3}.nodeID]);
ax2(4) = subplot(4,1,4);
hold on;
plot(myBeaconData{1,4}.timeRel/1000, myBeaconData{1,4}.maxRSSI, 'o', 'MarkerEdgeColor','b', 'MarkerFaceColor','b', 'MarkerSize',8);
plot(myBeaconData{2,4}.timeRel/1000, myBeaconData{2,4}.maxRSSI, 'o', 'MarkerEdgeColor','g', 'MarkerFaceColor','g', 'MarkerSize',8);
plot(myBeaconData{3,4}.timeRel/1000, myBeaconData{3,4}.maxRSSI, 'o', 'MarkerEdgeColor','r', 'MarkerFaceColor','r', 'MarkerSize',8);
hold off;
ylim([-100,-10]);
grid on;
xlabel('Time (s)');
ylabel(['NodeID: ' nodeData{4}.nodeID]);
linkaxes(ax2, 'x');


