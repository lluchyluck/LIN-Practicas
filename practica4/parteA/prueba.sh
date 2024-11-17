#!/bin/bash
for i in {1..100}; do
    echo "add $i" > /proc/modlist &
    echo "remove $i" > /proc/modlist &
done
