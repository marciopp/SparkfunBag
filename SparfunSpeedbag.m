cont=0; % hit count
delta=0; % acceleration instantaneous delta
finalmean=0; % acceleration smoothed
firstmean=90; % parameter for first moving average
secondmean=110; % parameter for second moving average
thirdmean=30; % parameter for third moving average
lowerlimit=80; % hits are above this limit
for i=1:length(x)-1; 
    delta(i)=abs(x(i+1)-x(i))+ abs(y(i+1)-y(i)) + abs(z(i+1)-z(i));
end;
finalmean=movmean(movmean(movmean(delta,firstmean,'omitnan'),secondmean,'omitnan'),thirdmean,'omitnan');
for i=1:length(finalmean)-2; 
    if (finalmean(i+2)<finalmean(i+1)) && (finalmean(i+1)>finalmean(i)) && (finalmean(i+1) > lowerlimit); 
        cont=cont+1; 
    end; 
end;
display(cont);
