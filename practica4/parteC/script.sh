#!/bin/bash

while true
do
   for i in {0..9} {a..f} {A..F} 
   do
      echo $i > /dev/display7s
      sleep 0.4
   done
done
pi@raspberr