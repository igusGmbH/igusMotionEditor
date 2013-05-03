// igus motion controller
// Author: Max Schwarz <max.schwarz@uni-bonn.de>

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/wdt.h>
#include <util/delay.h>
#include <util/atomic.h>
#include <stdio.h>

#include <stdint.h>
#include <string.h>

#include "uart.h"
#include "combuf.h"
#include "nanotec.h"
#include "protocol.h"
#include "mem.h"
#include "commands.h"
#include "motion.h"
#include "io.h"

// Setup stdio
static int pc_putc(char c, FILE* stream)
{
	if(c == '\n')
		pc_putc('\r', stream);

	com_buf_to_pc.put(c);

	uart_pc->startTransmitting();

	if(c == '\n')
	{
		while(com_buf_to_pc.available() || !uart_pc->dataSent())
			;
	}

	return 0;
}

uint8_t g_states[proto::NUM_AXES] = {-1};
bool g_javaRunning[proto::NUM_AXES] = {false};

static void initialize()
{
	uint8_t errorcnt = 0;

	while(1)
	{
		bool ready = true;

		_delay_ms(200);

		for(uint8_t i = 1; i <= mem_config.active_axes; ++i)
		{
			int8_t state = nt_state(i);
			g_states[i-1] = state;

			printf("% d ", state);

			if(state == -1)
			{
				if(++errorcnt == 200)
				{
					// A controller is not present. Go ahead anyway, but
					// disable motion playback.

					mem_config.num_keyframes = 0;
					return;
				}

				ready = false;
				continue;
			}

			if(state != NT_STATE_RESET)
				g_javaRunning[i-1] = true;

			if(!g_javaRunning[i-1])
			{
				g_javaRunning[i-1] = nt_startJava(i);
				if(g_javaRunning[i-1])
					printf("Started JAVA on %d\n", i);
				else
					printf("Failed to start JAVA on %d\n", i);

				ready = false;
				continue;
			}

			switch(state)
			{
				case NT_STATE_RESET:
					nt_setState(i, NT_STATE_SEARCH);
				case NT_STATE_SEARCH: // fallthrough intended
					ready = false;
					break;
				case NT_STATE_IDLE:
				case NT_STATE_COMPLIANCE:
					break;
			}
		}
		printf("\n");

		if(ready)
			break;
	}
}

int main()
{
	wdt_disable();

	proto::SimplePacket<proto::CMD_INIT> initPacket;

	uart_pc->init(BAUD_SETTING<115200>());
	uart_rob->init(16);

	io_init();
	nt_init();

	rs485_setDir(RS485_IN);

	PORTJ |= (1 << 7);

	FILE mystdio;
	fdev_setup_stream(&mystdio, pc_putc, NULL, _FDEV_SETUP_WRITE);
	stdout = &mystdio;

	sei();

	printf("Loading motion sequence\n");

	mem_init();
	motion_loadSequence();

	// Transmission PC -> RoboLink is handled in main(), since
	// we need to insert a short delay after switching on the
	// RS485 transmitter.

	for(uint16_t i = 0; i < 50; ++i)
		_delay_ms(10);

	printf("Starting up...\n");

	bool isInitialized = false;

	while(1)
	{
		if(io_button())
		{
			uart_setPassthroughEnabled(false);

			if(!isInitialized)
			{
				printf("Doing initialization\n");
				initialize();
				PORTJ &= ~(1 << 7);

				// Trigger first output command if we are in the starting position
				motion_isInStartPosition();
				isInitialized = true;
			}
			else if(!motion_isInStartPosition())
			{
				printf("Moving to start position\n");
				if(motion_doStartKeyframe())
					printf("success\n");
				else
					printf("failure\n");
			}
			else
				motion_runSequence();

			uart_setPassthroughEnabled(true);

			continue;
		}

		if(!com_buf_to_bot.available())
		{
			continue;
		}

		_delay_ms(20);

		rs485_setDir(RS485_OUT);
		_delay_us(20);
		_delay_us(20);

		// Check if this is an CMD_INIT packet
		uint8_t offset = 0;

		do
		{
			uint8_t c = com_buf_to_bot.get();
			if(c == initPacket[offset])
				offset++;
			else
			{
				// Replay matched bytes til now
				for(uint8_t i = 0; i < offset; ++i)
					uart_rob->put(initPacket[i]);

				uart_rob->put(c);
				offset = 0;
			}

			if(offset == sizeof(initPacket))
			{
				com_buf_to_bot.flush();

				PORTJ |= (1 << 7);

				rs485_setDir(RS485_IN);
				uart_setPassthroughEnabled(false);

				handleCommand(proto::CMD_INIT, 0, 0);
				handleCommands();

				uart_setPassthroughEnabled(true);

				rs485_setDir(RS485_OUT);

				com_buf_to_bot.flush();
			}
		}
		while(com_buf_to_bot.available());

		// Wait till transmission is complete
 		while(!uart_rob->dataSent());
		_delay_us(100);

		rs485_setDir(RS485_IN);
	}
}
