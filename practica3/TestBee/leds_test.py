#!/usr/bin/env python
import RPi.GPIO as GPIO
import time

#set GPIO numbering mode and define input pin
#ledgpios=[17,27,22]
ledgpios=[4,27,25]
GPIO.setmode(GPIO.BCM)
GPIO.setup(ledgpios[0],GPIO.OUT)
GPIO.setup(ledgpios[1],GPIO.OUT)
GPIO.setup(ledgpios[2],GPIO.OUT)

try:
    state=False
    while True:
        GPIO.output(ledgpios[0],state)
        GPIO.output(ledgpios[1],state)
        GPIO.output(ledgpios[2],state)        
        state=not state
        time.sleep(0.5)
finally:
    #cleanup the GPIO pins before ending
    GPIO.cleanup()
