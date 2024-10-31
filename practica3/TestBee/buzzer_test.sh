#/bin/bash
PWMDIR=/sys/class/pwm/pwmchip0
PWM0DIR=/sys/class/pwm/pwmchip0/pwm0

if [ ! -d ${PWMDIR} ]; then
	echo "$PWMDIR directory not found" 
	exit 1
fi


echo "Exporting PWM0 channel"
cd $PWMDIR
echo 0 > export

if [ ! -d ${PWM0DIR} ]; then
	echo "$PWM0DIR directory not found. PWM channel unavailable" 
	exit 2
fi

echo "Testing Passive Buzzer"
echo -n "A4 .. "
echo 2272720 > ${PWM0DIR}/period
echo 1136360 > ${PWM0DIR}/duty_cycle
echo 1 > ${PWM0DIR}/enable
sleep 1
echo 0 > ${PWM0DIR}/enable
echo -n "E4 .. "
echo 3033704 > ${PWM0DIR}/period
echo 1516852 > ${PWM0DIR}/duty_cycle
echo 1 > ${PWM0DIR}/enable
sleep 1
echo 0 > ${PWM0DIR}/enable
echo "A4"
echo 2272720 > ${PWM0DIR}/period
echo 1136360 > ${PWM0DIR}/duty_cycle
echo 1 > ${PWM0DIR}/enable
sleep 1
echo 0 > ${PWM0DIR}/enable
echo "Done"


## Disabling PWM Channel

cd $PWMDIR
echo "Unexporting PWM0 channel"
echo 0 > unexport