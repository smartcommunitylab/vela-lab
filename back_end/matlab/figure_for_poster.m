figure;
demo = 0;
EXPECTED_REPORT_INTERVAL_S = 5;


if demo
    nodeOrder = [2,3,4,1];%1:size(myBeaconData,2);
else
    nodeOrder = 1:size(myBeaconData,2);
end

for nodeNo = nodeOrder
    legendStr = [];
    ax2(nodeNo) = subplot(size(myBeaconData,2),1,find(nodeNo == nodeOrder));
    hold on;
    for beaconNo = 1:size(myBeaconData,1)
        NaNtimeData = myBeaconData{beaconNo,nodeNo}.timeData(diff(myBeaconData{beaconNo,nodeNo}.timeData) > seconds(3.1*EXPECTED_REPORT_INTERVAL_S)) + seconds(EXPECTED_REPORT_INTERVAL_S/10);
        data_plot = [myBeaconData{beaconNo,nodeNo}.maxRSSI ; NaN*ones(size(NaNtimeData))];
        time_plot = [myBeaconData{beaconNo,nodeNo}.timeData; NaNtimeData];
        [time_plot, o] = sort(time_plot);
        data_plot = data_plot(o);
        p = plot(time_plot, data_plot, '-o', 'MarkerSize',8, 'LineWidth',2);
        p.MarkerFaceColor = p.Color;
        legendStr = [legendStr; 'Beacon: 0x' myBeaconID{beaconNo}(end-1:end)];
    end
    if(find(nodeNo == nodeOrder) == 1)
        l = legend(legendStr);
        l.FontSize = 12;
        l.FontWeight = 'bold';
    end
    ax2(nodeNo).FontSize=12;
    ax2(nodeNo).FontWeight='bold';
    ax2(nodeNo).YTick=MIN_RSSI_SCALE:10:MAX_RSSI_SCALE;
    %ax2(nodeNo).XTick=datenum(2017,12,19,11,50,30):datenum(seconds(15)):datenum(2017,12,19,11,55,00);
    hold off;
    ylim([MIN_RSSI_SCALE,MAX_RSSI_SCALE]);
    grid on;
    if demo
        xlim([datenum(2017,12,19,11,50,30), datenum(2017,12,19,11,55,00)]);
        l = ylabel(['NodeID: ' sprintf('%d\n',find(nodeNo == nodeOrder)) 'RSSI [dBm]']);
    else
        l = ylabel(['NodeID: ' nodeData{(nodeNo == nodeOrder)}.nodeID sprintf('\n') 'RSSI [dBm]']);
    end
    l.FontSize = 12;
    l.FontWeight = 'bold';
end
linkaxes(ax2, 'xy');

% NaNtimeData = myBeaconData{2,1}.timeData(diff(myBeaconData{2,1}.timeData) > seconds(50)) + seconds(1);
% data_plot = [myBeaconData{2,1}.maxRSSI ; NaN*ones(size(NaNtimeData))];
% time_plot = [myBeaconData{2,1}.timeData; NaNtimeData];
% [time_plot, o] = sort(time_plot);
% data_plot = data_plot(o);
% plot(time_plot, data_plot, '-o', 'MarkerEdgeColor','g', 'MarkerFaceColor','g', 'MarkerSize',8,'Color','g');
% NaNtimeData = myBeaconData{3,1}.timeData(diff(myBeaconData{3,1}.timeData) > seconds(50)) + seconds(1);
% data_plot = [myBeaconData{3,1}.maxRSSI ; NaN*ones(size(NaNtimeData))];
% time_plot = [myBeaconData{3,1}.timeData; NaNtimeData];
% [time_plot, o] = sort(time_plot);
% data_plot = data_plot(o);
% plot(time_plot, data_plot, '-o', 'MarkerEdgeColor','r', 'MarkerFaceColor','r', 'MarkerSize',8,'Color','r');
% hold off;
% ylim([MIN_RSSI_SCALE,MAX_RSSI_SCALE]);
% xlim([datenum(2017,12,19,11,50,30), datenum(2017,12,19,11,55,00)]);
% grid on;
% ylabel('NodeID: 4 RSSI [dBm]');
% legend(myBeaconID{1}, myBeaconID{2}, myBeaconID{3});

% ax2(2) = subplot(4,1,1);
% hold on;
% NaNtimeData = myBeaconData{1,2}.timeData(diff(myBeaconData{1,2}.timeData) > seconds(50)) + seconds(1);
% data_plot = [myBeaconData{1,2}.maxRSSI ; NaN*ones(size(NaNtimeData))];
% time_plot = [myBeaconData{1,2}.timeData; NaNtimeData];
% [time_plot, o] = sort(time_plot);
% data_plot = data_plot(o);
% plot(time_plot, data_plot, '-o', 'MarkerEdgeColor','b', 'MarkerFaceColor','b', 'MarkerSize',8,'Color','b');
% NaNtimeData = myBeaconData{2,2}.timeData(diff(myBeaconData{2,2}.timeData) > seconds(50)) + seconds(1);
% data_plot = [myBeaconData{2,2}.maxRSSI ; NaN*ones(size(NaNtimeData))];
% time_plot = [myBeaconData{2,2}.timeData; NaNtimeData];
% [time_plot, o] = sort(time_plot);
% data_plot = data_plot(o);
% plot(time_plot, data_plot, '-o', 'MarkerEdgeColor','g', 'MarkerFaceColor','g', 'MarkerSize',8,'Color','g');
% NaNtimeData = myBeaconData{3,2}.timeData(diff(myBeaconData{3,2}.timeData) > seconds(50)) + seconds(1);
% data_plot = [myBeaconData{3,2}.maxRSSI ; NaN*ones(size(NaNtimeData))];
% time_plot = [myBeaconData{3,2}.timeData; NaNtimeData];
% [time_plot, o] = sort(time_plot);
% data_plot = data_plot(o);
% plot(time_plot, data_plot, '-o', 'MarkerEdgeColor','r', 'MarkerFaceColor','r', 'MarkerSize',8,'Color','r');
% hold off;
% ylim([MIN_RSSI_SCALE,MAX_RSSI_SCALE]);
% xlim([datenum(2017,12,19,11,50,30), datenum(2017,12,19,11,55,00)]);
% grid on;
% ylabel('NodeID: 1 RSSI [dBm]');
% legend(myBeaconID{1}, myBeaconID{2}, myBeaconID{3});
% 
% ax2(3) = subplot(4,1,2);
% hold on;
% NaNtimeData = myBeaconData{1,3}.timeData(diff(myBeaconData{1,3}.timeData) > seconds(50)) + seconds(1);
% data_plot = [myBeaconData{1,3}.maxRSSI ; NaN*ones(size(NaNtimeData))];
% time_plot = [myBeaconData{1,3}.timeData; NaNtimeData];
% [time_plot, o] = sort(time_plot);
% data_plot = data_plot(o);
% plot(time_plot, data_plot, '-o', 'MarkerEdgeColor','b', 'MarkerFaceColor','b', 'MarkerSize',8,'Color','b');
% NaNtimeData = myBeaconData{2,3}.timeData(diff(myBeaconData{2,3}.timeData) > seconds(50)) + seconds(1);
% data_plot = [myBeaconData{2,3}.maxRSSI ; NaN*ones(size(NaNtimeData))];
% time_plot = [myBeaconData{2,3}.timeData; NaNtimeData];
% [time_plot, o] = sort(time_plot);
% data_plot = data_plot(o);
% plot(time_plot, data_plot, '-o', 'MarkerEdgeColor','g', 'MarkerFaceColor','g', 'MarkerSize',8,'Color','g');
% NaNtimeData = myBeaconData{3,3}.timeData(diff(myBeaconData{3,3}.timeData) > seconds(50)) + seconds(1);
% data_plot = [myBeaconData{3,3}.maxRSSI ; NaN*ones(size(NaNtimeData))];
% time_plot = [myBeaconData{3,3}.timeData; NaNtimeData];
% [time_plot, o] = sort(time_plot);
% data_plot = data_plot(o);
% plot(time_plot, data_plot, '-o', 'MarkerEdgeColor','r', 'MarkerFaceColor','r', 'MarkerSize',8,'Color','r');
% hold off;
% ylim([MIN_RSSI_SCALE,MAX_RSSI_SCALE]);
% xlim([datenum(2017,12,19,11,50,30), datenum(2017,12,19,11,55,00)]);
% grid on;
% ylabel('NodeID: 2 RSSI [dBm]');
% 
% ax2(4) = subplot(4,1,3);
% hold on;
% NaNtimeData = myBeaconData{1,4}.timeData(diff(myBeaconData{1,4}.timeData) > seconds(50)) + seconds(1);
% data_plot = [myBeaconData{1,4}.maxRSSI ; NaN*ones(size(NaNtimeData))];
% time_plot = [myBeaconData{1,4}.timeData; NaNtimeData];
% [time_plot, o] = sort(time_plot);
% data_plot = data_plot(o);
% plot(time_plot, data_plot, '-o', 'MarkerEdgeColor','b', 'MarkerFaceColor','b', 'MarkerSize',8,'Color','b');
% NaNtimeData = myBeaconData{2,4}.timeData(diff(myBeaconData{2,4}.timeData) > seconds(50)) + seconds(1);
% data_plot = [myBeaconData{2,4}.maxRSSI ; NaN*ones(size(NaNtimeData))];
% time_plot = [myBeaconData{2,4}.timeData; NaNtimeData];
% [time_plot, o] = sort(time_plot);
% data_plot = data_plot(o);
% plot(time_plot, data_plot, '-o', 'MarkerEdgeColor','g', 'MarkerFaceColor','g', 'MarkerSize',8,'Color','g');
% NaNtimeData = myBeaconData{3,4}.timeData(diff(myBeaconData{3,4}.timeData) > seconds(50)) + seconds(1);
% data_plot = [myBeaconData{3,4}.maxRSSI ; NaN*ones(size(NaNtimeData))];
% time_plot = [myBeaconData{3,4}.timeData; NaNtimeData];
% [time_plot, o] = sort(time_plot);
% data_plot = data_plot(o);
% plot(time_plot, data_plot, '-o', 'MarkerEdgeColor','r', 'MarkerFaceColor','r', 'MarkerSize',8,'Color','r');
% hold off;
% ylim([MIN_RSSI_SCALE,MAX_RSSI_SCALE]);
% xlim([datenum(2017,12,19,11,50,30), datenum(2017,12,19,11,55,00)]);
% grid on;
% xlabel('Time');
% ylabel('NodeID: 3 RSSI [dBm]');
% linkaxes(ax2, 'xy');
