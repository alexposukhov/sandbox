#!/bin/bash -e

export CROSS_COMPILE=${HOME}/BBB/gcc-linaro-arm-linux-gnueabihf-4.9-2014.09_linux/bin/arm-linux-gnueabihf-

echo ${CROSS_COMPILE}


${CROSS_COMPILE}gcc analog_clock_color.c -o analog_clock_color -lm


echo "Done!"
