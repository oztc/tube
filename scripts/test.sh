#!/bin/bash

START=`date +%s%N`
ITER=5000
for ((i=0;i<$ITER;i++)); do
    echo "sssssaaaassssssssssfffffffffffffffddddddddddddddd" | nc 192.172.1.4 7000 > /dev/null &
done
wait
END=`date +%s%N`
DIFF=`expr \( $END - $START \) / 1000000`
echo "$ITER done in $DIFF ms"

