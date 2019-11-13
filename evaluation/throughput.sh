#!/bin/bash

path=/home/lkwangh/project/ZoneStore/build

arr=(64 256 1024 4096 16384 65536 262144)
#arr=(1024 4096 16384)
#arr=(65536 262144)
# 100GB
#data=`expr 100 \* 1024 \* 1024 \* 1024`
# 1GB
data=`expr 1 \* 1024 \* 1024 \* 1024`

for v in ${arr[@]}
do
    echo v$v start

    byte=`expr $v + 16`
    num=`expr $data / $byte`
    echo $num

    nvme format /dev/nvme0n1
    #blktrace -d /dev/nvme0n1 -a complete -o - | blkparse -i - -o blktrace/v${v}B_bt &
    sleep 1
    
    $path/db_bench --benchmarks="fillrandom,readrandom,stats" --histogram=1 --value_size=$v --num=$num --compression_ratio=1 --db=/dev/nvme0n1 > v${v}B

    #pid=`ps aux | awk '/blktrace/ {print $2}' | head -n 1`
    
    #kill $pid
    
    #echo "v$v end $pid"
    echo "v$v end"

    sleep 1
done
