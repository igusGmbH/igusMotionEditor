// Nanotec motor controller driver
// Author: Max Schwarz <max.schwarz@uni-bonn.de>

#include "nanotec.h"

#include "combuf.h"
#include "uart.h"
#include "protocol.h"

#include <util/delay.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/**
 * The nanotec communication protocol is hideously slow. It uses 115200 Baud
 * speed and ASCII coding to transmit commands. You might need 4 characters to
 * transmit a signed byte!
 *
 * To compensate for the crappy protocol we use heavy buffering/caching.
 * If we are sure we sent a command value before, do not send the same value
 * again. This is not very failure-proof, but smooth playback is more important.
 **/

#define USE_BUFFER 0

#if USE_BUFFER
// Buffer structure
struct ControllerBuffer
{
	uint16_t dest;
	uint16_t velocity;
};

ControllerBuffer g_ctl_buffer[proto::NUM_AXES];
#endif

void nt_init()
{
#if USE_BUFFER
	// Set impossible values
	for(uint8_t i = 0; i < proto::NUM_AXES; ++i)
	{
		g_ctl_buffer[i].dest = 0xFFFF;
		g_ctl_buffer[i].velocity = 0xFFFF;
	}
#endif
}

static void write(char c)
{
	uart_rob->put(c);
}

static void write(const char* c)
{
	rs485_setDir(RS485_OUT);
	_delay_us(200);

	while(*c)
		uart_rob->put(*(c++));

	uart_rob->put('\r');

	// Wait till transmission is complete
	while(!uart_rob->dataSent());
	_delay_us(200);

	rs485_setDir(RS485_IN);
}

static uint8_t readResponse(char* dest, uint8_t max_len)
{
	uint8_t cnt = 0;
	dest[max_len-1] = '\0';

	while(1)
	{
		uint8_t timeout = 0;
		while(!com_buf_from_bot.available())
		{
			_delay_us(30);
			timeout++;
			if(timeout == 255)
				return 0; // timeout
		}

		uint8_t c = com_buf_from_bot.get();
		if(c == '\r')
		{
			if(dest && cnt < max_len-1)
				dest[cnt] = '\0';
			return cnt;
		}

		if(dest && cnt < max_len-1)
			dest[cnt] = c;

		cnt++;
	}
}

static bool isResponse(const char* cmp)
{
	char respbuf[20];
	uint8_t count = readResponse(respbuf, sizeof(respbuf));

	if(count < strlen(cmp))
		return false;

	if(strncmp(cmp, respbuf, strlen(cmp)) == 0)
		return true;
	else
		return false;
}

static bool chat(const char* command, const char* expected_answer)
{
	write(command);

	return isResponse(expected_answer);
}

bool nt_ping(uint8_t id)
{
	char cmd_buf[20];
	char answer_buf[20];

	snprintf(cmd_buf, sizeof(cmd_buf), "#%dZP", id);
	snprintf(answer_buf, sizeof(answer_buf), "%dZP+", id);

	return chat(cmd_buf, answer_buf);
}

static bool readRegister(uint8_t id, char reg, int16_t* ret)
{
	char cmd_buf[20];
	char answer_buf[20];

	snprintf(cmd_buf, sizeof(cmd_buf), "#%dZ%c", id, reg);
	write(cmd_buf);

	uint8_t response_len = readResponse(answer_buf, sizeof(answer_buf));

	if(response_len < 4)
		return false;

	if(answer_buf[0] != '0' + id)
		return false;

	if(answer_buf[1] != 'Z' || answer_buf[2] != reg)
		return false;

	if(answer_buf[3] == '\0')
		return false;

	char* endptr;
	int16_t tmp = strtol(answer_buf + 3, &endptr, 10);
	if(*endptr != '\0')
		return false;

	*ret = tmp;

	return true;
}

int8_t nt_state(uint8_t id)
{
	int16_t value;
	if(!readRegister(id, 'P', &value))
		return -1;

	if(value >= NT_STATE_COUNT)
		return -1;

	return value;
}

void nt_setState(uint8_t id, uint8_t state)
{
	char cmd_buf[20];

	snprintf(cmd_buf, sizeof(cmd_buf), "#%dP%d", id, state);
	write(cmd_buf);

	readResponse(NULL, 0);
}

bool nt_startJava(uint8_t id)
{
	char cmd_buf[20];
	char answer_buf[20];

	snprintf(cmd_buf, sizeof(cmd_buf), "#%d(JA", id);
	snprintf(answer_buf, sizeof(answer_buf), "%d(JA+", id);

	return chat(cmd_buf, answer_buf);
}

void nt_setDestination(uint8_t id, uint16_t dest)
{
#if USE_BUFFER
	if(g_ctl_buffer[id-1].dest == dest)
		return;
#endif

	char cmd_buf[20];

	snprintf(cmd_buf, sizeof(cmd_buf), "#%dn%u", id, dest);

	if(chat(cmd_buf, cmd_buf + 1))
#if USE_BUFFER
		g_ctl_buffer[id-1].dest = dest;
#else
		;
#endif
}

void nt_setVelocity(uint8_t id, uint16_t vel)
{
#if USE_BUFFER
	if(g_ctl_buffer[id-1].velocity == vel)
		return;
#endif

	char cmd_buf[20];

	snprintf(cmd_buf, sizeof(cmd_buf), "#%do%u", id, vel);

	if(chat(cmd_buf, cmd_buf + 1))
#if USE_BUFFER
		g_ctl_buffer[id-1].velocity = vel;
#else
		;
#endif
}

bool nt_encoderPosition(uint8_t id, int16_t* value)
{
	return readRegister(id, 'I', value);
}

bool nt_command(uint8_t id, int16_t* value)
{
	return readRegister(id, 's', value);
}

