###
 # @Description: -
 # @Version: -
 # @Author: Fox_benjiaming
 # @Date: 2020-10-06 01:19:27
 # @LastEditors: Fox_benjiaming
 # @LastEditTime: 2021-05-09 21:57:55
### 
#!/bin/bash

ip=192.168.1.108
bin_dir=/root/bin
drv_dir=/root/driver
bin=0
drv=0

if [ $# -ne 1 ] && [ $# -ne 2 ]; then
    echo "Usage:"
    echo "  ./push.sh folder [bin|drv]"
    echo "Example: ./push.sh main/ drv"
    exit 1
else
    # folder=$1
    cd $1
    pwd
    # cd `pwd`/$folder
fi

if [ -z $2 ]; then
    echo "I will push bin and drv!"
    bin=1
    drv=1
elif [ $2 = "bin" ]; then
    echo "I will push bin!"
    bin=1
elif [ $2 = "drv" ]; then
    echo "I will push drv!"
    drv=1
else
    echo "I only accept bin or drv!"
    exit 1
fi

first_name=`pwd | awk -F / '{ print $NF }'`
# if [ $bin = "1" ] && [ $drv = "1" ]; then
#     scp "$first_name"_test "$first_name"_drv.ko root@$ip:$bin_dir
# elif [ $drv = "1" ] && [ $drv = "0"]; then
#     scp "$first_name"_test root@$ip:$bin_dir
# elif [ $drv = "0" ] && [ $drv = "1"]; then
#     scp "$first_name"_drv.ko root@$ip:$bin_dir
# else
#     echo "No file!"
if [ $bin = "1" ]; then
    scp "$first_name"_test root@$ip:$bin_dir
fi
if [ $drv = "1" ]; then
    scp "$first_name"_drv.ko root@$ip:$drv_dir
fi
# fi
