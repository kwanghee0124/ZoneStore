#!/bin/bash

file=`echo $1 | sed 's/[^0-9B]//g'`
filew=${file}_write.dat

old=0
cnt=0
while read line
do
    div=`echo $line | cut -d' ' -f1`
    if [ "$div" == "[" ]; then
        latency=`echo $line | sed 's/[^0-9. ]//g' | cut -d' ' -f2`
        per=`echo $line | sed 's/[^0-9. ]//g' | cut -d' ' -f7`
        
        if [ $old -gt $latency ]; then 
            filew=${file}_read.dat
            old=$latency
            cnt=0
        else
            old=$latency
        fi

        latency=`expr $latency \* 1000`
        if [ $cnt -eq 0 ]; then
            echo "$latency 0" >> ./plot_data/$filew
        fi
        echo "$latency $per" >> ./plot_data/$filew
        cnt=`expr $cnt + 1`
    fi
done < $1
