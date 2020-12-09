###
 # @Description: -
 # @Version: -
 # @Author: Fox_benjiaming
 # @Date: 2020-10-21 06:24:51
 # @LastEditors: Fox_benjiaming
 # @LastEditTime: 2020-10-22 04:58:32
### 
#!/bin/bash
DATA_FILE_PATH=/root/card_recorder.txt
LOG_FILE_PATH=/var/log/tv_log.txt
# STATUS_FILE_PATH=/root/tv/status.txt
RPiTVPowerOn=/root/bin/RPiTVPowerOn

last_time="20:00:00"

start_str1=$(date +%Y-%m-%d)
start_str2="08:00:00"
# echo "T" > $STATUS_FILE_PATH
echo "$start_str1 $start_str2 : Morning!Checking card..." >> $LOG_FILE_PATH

while [ 1 -gt 0 ]; do 
    check_str1=$(cut -d " " -f 1 $DATA_FILE_PATH)
    check_str2=$(cut -d " " -f 2 $DATA_FILE_PATH)
    currnet_time=$(date +%H:%M:%S)
    if [ $start_str1 \< $check_str1 ] || [ $start_str1 = $check_str1 ]; then
        if [ $start_str2 \< $check_str2 ] || [ $start_str2 = $check_str2 ]; then
            echo "Checked card!" >> $LOG_FILE_PATH
            $RPiTVPowerOn 
            # echo "F" > $STATUS_FILE_PATH
            break
        fi
    fi
    if [ $currnet_time \> $last_time ]; then
        echo "Don't checked card!" >> $LOG_FILE_PATH
        # echo "F" > $STATUS_FILE_PATH
        break
    fi

    sleep 1s
done
