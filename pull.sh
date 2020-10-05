#!/bin/bash

ip=192.168.1.107
bin_dir=/root/bin
drv_dir=/root/driver

echo `ls ./bin`
echo `ls ./driver`

if [ -z $1 ]; then
    scp ./bin/* root@$ip:$bin_dir
    scp ./driver/* root@$ip:$drv_dir
    # cd ./driver
    # scp `ls` root@$ip:$drv_dir
    # cd ../bin
    # scp `ls` root@$ip:$bin_dir
elif [ $1 = "drv" ]; then
    cd ./driver
    scp `ls` root@$ip:$drv_dir
elif [ $1 = "bin" ]; then
    cd ./bin
    scp `ls` root@$ip:$bin_dir
fi