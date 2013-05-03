#!/bin/bash

port="$1"
file="$2"

if [[ ! -w "$port" ]]; then
	echo "Waiting for serial port to appear..."
	while [[ ! -w "$port" ]]; do
		true
	done
fi

./genreset "$port"
avrdude -c avr109 -p atmega2560 -b 115200 -P "$port" -U flash:w:"$2"
