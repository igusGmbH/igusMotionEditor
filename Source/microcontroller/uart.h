// UART driver & RS485 arbitration handling
// Author: Max Schwarz <max.schwarz@uni-bonn.de>

#ifndef UART_H
#define UART_H

#include <stdint.h>
#include <avr/io.h>

struct UART
{
	volatile uint8_t control_a;
	volatile uint8_t control_b;
	volatile uint8_t control_c;

	uint8_t pad0;

	uint16_t baud;

	volatile uint8_t data;

	void init(uint16_t baud_setting);

	inline void put(uint8_t c)
	{
		while(!(control_a & (1 << UDRE0)));
		control_a |= (1 << TXC0);
		data = c;
	}

	inline void startTransmitting()
	{
		control_b |= (1 << UDRIE0);
	}

	inline uint8_t getc()
	{
		return data;
	}

	inline bool dataAvailable()
	{
		return control_a & (1 << RXC0);
	}

	inline bool dataSent()
	{
		return control_a & (1 << TXC0);
	}
} __attribute__((packed));

extern UART* uarts[];

UART* const uart_pc = uarts[0];
UART* const uart_rob = uarts[3];

template<long int baud> uint16_t BAUD_SETTING();

template<>
inline uint16_t BAUD_SETTING<115200>() { return 16; }

enum RS485Direction
{
	RS485_IN,
	RS485_OUT
};

inline void rs485_setDir(RS485Direction dir)
{
	if(dir == RS485_IN)
		PORTJ &= ~(1 << 2);
	else
		PORTJ |= (1 << 2);
}

void uart_setPassthroughEnabled(bool enabled);

#endif
