// Digital I/O
// Author: Max Schwarz <max.schwarz@uni-bonn.de>

#include "io.h"

#include <avr/io.h>
#include <util/delay.h>

void io_init()
{
	// Connections:
	//   PORTC:
	//     0-1: Digital output (LED)
	//     2: synchronize pin
	//   PORTJ:
	//     2: RS485 direction (1=output)
	//     7: board LED
	//   PORTG:
	//     0-1: Start button/switch

	DDRJ = (1 << 2) | (1 << 7);

	PORTG = (1 << 1); // enable pullup for switch
	DDRG = (1 << 0);  // GND for switch

	PORTC = 0;
	DDRC = (1 << 0) | (1 << 1) | (1 << 2);
}

bool io_button()
{
	return !(PING & (1 << 1));
}

void io_setOutput(bool active)
{
	if(active)
		PORTC |= (1 << 0);
	else
		PORTC &= ~(1 << 0);
}

void io_synchronize()
{
	// Release sync line and enable pullup
	DDRC &= ~(1 << 2);
	PORTC |= (1 << 2);

	uint8_t counter = 0;

	while(1)
	{
		if(PINC & (1 << 2))
			counter++;
		else
			counter = 0;

		if(counter > 20)
			break;
	}

	// Ensure that the other controllers have a chance to see the high level
	_delay_ms(20);

	// Reassert sync line
	PORTC &= ~(1 << 2);
	DDRC |= (1 << 2);
}
