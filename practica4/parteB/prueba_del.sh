#!/bin/bash

# Consumer script to read numbers from /dev/prodcons
for i in {1..11}; do
    cat /dev/prodcons

    sleep 0.5
done
