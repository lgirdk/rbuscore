RBUS_LOG_FILE="/rdklogs/logs/rtrouted.log"
LAST_RBUS_LOG_TIME="/tmp/lastrtroutedlogtime"
current_time=0
lastync_time=0
BootupLog_is_updated=0
while [ 1 ]
do
   current_time=$(date +%s)
   if [ -f "$LAST_RBUS_LOG_TIME" ];then
        lastsync_time=`cat $LAST_RBUS_LOG_TIME`
   fi
   difference_time=$(( current_time - lastsync_time ))
   lastsync_time=$current_time
   echo "$current_time" > $LAST_RBUS_LOG_TIME
   #Keep appending to the existing file
   nice -n 19 journalctl -u rbus --since "${difference_time} sec ago" | grep -v "\-\- Logs begin at " >> ${RBUS_LOG_FILE}
   sleep 10
done;
