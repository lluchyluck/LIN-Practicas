#!/usr/bin/env python

#================================================
#
#
#   Extend use of 8 LED with 74HC595.
#   Change the  WhichLeds and sleeptime value under
#   loop() function to change LED mode and speed.
#
#=================================================

import RPi.GPIO as GPIO
import time

SDI   = 18 
RCLK  = 23 
SRCLK = 24 

A=0x80
B=0x40
C=0x20
D=0x10
E=0x08
F=0x04
G=0x02
DP=0x01

D0=(A|B|C|D|E|F)
D1=(B|C)
D2=(A|B|G|D|E)
D3=(A|B|C|D|G)
D4=(B|C|F|G)
D5=(A|F|G|C|D)
D6=(A|C|D|E|F|G)
D7=(A|B|C)
D8=(A|B|C|D|E|F|G)
D9=(A|B|C|F|G)
DA=(A|B|C|E|F|G)
DB=(C|D|E|F|G)
DC=(A|D|E|F)
DD=(B|C|D|E|G)
DE=(A|D|E|F|G)
DF=(A|E|F|G)

NUMCODES=[D0,D1,D2,D3,D4,D5,D6,D7,D8,D9,DA,DB,DC,DD,DE,DF]

#=================================================

def print_message():
    print ("========================================")
    print ("|           LEDs with 74HC595          |")
    print ("|                                      |")
    print ("|       Control LEDs with 74HC595      |")
    print ("|                                      |")
    print ("========================================\n")
    print ('Program is running...')
    print ('Please press Ctrl+C to end the program...')

def setup():
    GPIO.setmode(GPIO.BCM)    # Number GPIOs by its BCM location
    GPIO.setup(SDI, GPIO.OUT, initial=GPIO.LOW)
    GPIO.setup(RCLK, GPIO.OUT, initial=GPIO.LOW)
    GPIO.setup(SRCLK, GPIO.OUT, initial=GPIO.LOW)

# Shift the data to 74HC595
def update_7sdisplay(dat):
    showdata=[]
    for bit in range(0, 8):
        GPIO.output(SDI, 0x80 & (dat << bit))
        GPIO.output(SRCLK, GPIO.HIGH)
        time.sleep(0.001)
        GPIO.output(SRCLK, GPIO.LOW)
    GPIO.output(RCLK, GPIO.HIGH)
    time.sleep(0.001)
    GPIO.output(RCLK, GPIO.LOW)


def main():
    print_message()
    sleeptime = 0.4        # Change speed, lower value, faster speed
    blink_sleeptime = 1.0
    leds = ['-', '-', '-', '-', '-', '-', '-', '-']
    while True:
        for code in NUMCODES:
            update_7sdisplay(code)
            time.sleep(sleeptime)        


def destroy():
    update_7sdisplay(0)
    GPIO.cleanup()

if __name__ == '__main__':
    setup()
    try:
        main()
    except KeyboardInterrupt:
        destroy()
