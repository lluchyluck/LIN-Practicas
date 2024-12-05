#!/bin/bash
for i in {1..100}; do

    echo "remove $i" > /proc/modlist 
    cat /proc/modlist
    sleep 0.1
done

