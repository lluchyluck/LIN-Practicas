#!/bin/bash
for i in {1..100}; do
    echo "add $i" > /proc/modlist
    sleep 0.1
done
