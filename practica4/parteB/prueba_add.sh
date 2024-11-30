#!/bin/bash

# Producer script to insert numbers into /dev/prodcons
for i in {1..10}; do
    echo "insertando $i"
    echo "$i" > /dev/prodcons
    sleep 0.5
done
