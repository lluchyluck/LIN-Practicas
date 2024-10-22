#!/bin/bash

while true
do
   for (( i=0; $i<8 ; i++ ))
   do
      sudo ./ledctl_invoke $i
      sleep 0.5
    done
done