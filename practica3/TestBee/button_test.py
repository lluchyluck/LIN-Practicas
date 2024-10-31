#!/usr/bin/env python
import RPi.GPIO as GPIO
import time

#set GPIO numbering mode and define input pin
#pinnumber=4
pinnumber=22
GPIO.setmode(GPIO.BCM)
GPIO.setup(pinnumber,GPIO.IN)

try:
    while True:
        if GPIO.input(pinnumber)==0:
            print("Open")
        else:
            print("Closed")
        time.sleep(0.2)

finally:
    #cleanup the GPIO pins before ending
    GPIO.cleanup()
