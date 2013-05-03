// Nanotec motor controller driver
// Author: Max Schwarz <max.schwarz@uni-bonn.de>

#ifndef NANOTEC_H
#define NANOTEC_H

#include <stdint.h>

enum NanotecState
{
	NT_STATE_RESET = 0,
	NT_STATE_SEARCH = 1,
	NT_STATE_IDLE = 2,
	NT_STATE_COMPLIANCE = 3,

	NT_STATE_COUNT
};

void nt_init();

bool nt_ping(uint8_t id);
int8_t nt_state(uint8_t id);
void nt_setState(uint8_t id, uint8_t state);

bool nt_startJava(uint8_t id);

void nt_setDestination(uint8_t id, uint16_t dest);
void nt_setVelocity(uint8_t id, uint16_t vel);

bool nt_encoderPosition(uint8_t id, int16_t* dest);
bool nt_command(uint8_t id, int16_t* dest);

#endif
