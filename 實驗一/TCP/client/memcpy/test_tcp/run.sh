#!/bin/bash
# Program:
#	execute 50 receivers at the time
PATH=/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin:/usr/local/sbin:~/bin
export PATH
i=1
while [ "$i" -le "50" ]
do
echo "Client" $i "generate"
i=$(($i+1))
done
echo "finish"

