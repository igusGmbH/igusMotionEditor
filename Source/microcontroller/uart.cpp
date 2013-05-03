// UART driver & RS485 arbitration handling
// Author: Max Schwarz <max.schwarz@uni-bonn.de>

#include "uart.h"

#include "combuf.h"

#include <avr/interrupt.h>
#include <util/delay.h>
#include <util/atomic.h>

UART* uarts[] = {
	(UART*)0xC0,
	(UART*)0xC8,
	(UART*)0xD0,
	(UART*)0x130
};

volatile bool g_passthrough = true;

void UART::init(uint16_t baud_setting)
{
	baud = baud_setting;

	control_a = (1 << U2X0);
	control_b = (1 << RXEN0) | (1 << TXEN0) | (1 << RXCIE0);
	control_c = (3 << UCSZ00);
}

void uart_setPassthroughEnabled(bool enabled)
{
	g_passthrough = enabled;
}

// UART interrupts

ISR(USART0_RX_vect)
{
	com_buf_to_bot.put(UDR0);
}

ISR(USART0_UDRE_vect)
{
	if(com_buf_to_pc.available())
	{
		UDR0 = com_buf_to_pc.get();
	}
	else
	{
		UCSR0B &= ~(1 << UDRIE0);
	}
}

ISR(USART3_RX_vect)
{
	uint8_t c = UDR3;
	if(!c)
		return;

	if(g_passthrough)
	{
		com_buf_to_pc.put(c);
		UCSR0B |= (1 << UDRIE0);
	}
	else
		com_buf_from_bot.put(c);
}

