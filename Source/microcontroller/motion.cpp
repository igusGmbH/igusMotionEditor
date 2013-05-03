// Motion control
// Author: Max Schwarz <max.schwarz@uni-bonn.de>

#include "motion.h"

#include "protocol.h"
#include "uart.h"
#include "mem.h"
#include "nanotec.h"
#include "io.h"
#include "combuf.h"
#include "commands.h"

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include <util/delay.h>

#include <stdio.h>
#include <stdlib.h>

const bool MOTION_PLOT = false;
const bool PLOT_STOP = false;
const bool SYNCHRONIZE = true;

// Timer design: g_ticks increases once every 1ms.
volatile static uint32_t g_ticks;
volatile static uint32_t g_delta; //!< Reset to zero on resetTimer()
volatile static uint32_t g_dest;
volatile static bool g_reached;

proto::Keyframe g_buffer[proto::MAX_KEYFRAMES];
bool g_shouldStop;
bool g_isPlaying;
int16_t g_encPos[proto::NUM_AXES];

ISR(TIMER1_COMPA_vect)
{
	g_delta++;
	g_ticks++;

	if(!g_reached && g_ticks >= g_dest)
		g_reached = true;

	if(g_ticks % 128 == 0)
		PORTJ ^= (1 << 7);
}

static void resetTimer(uint32_t dest)
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		g_dest = g_ticks + dest;
		g_delta = 0;
		g_reached = false;
	}
}

static void startTimer()
{
	g_ticks = 0;

	OCR1A = 250-1;
	TCCR1A = 0;
	TIMSK1 = (1 << OCIE1A);
	// Prescaler: 64, CTC mode
	TCCR1B = (1 << WGM12) | (1 << CS11) | (1 << CS10);
	OCR1A = 250-1;
	TCNT1 = 0;
}

static void stopTimer()
{
	TCCR1B = 0;
}

static uint32_t getTicks()
{
	uint32_t copy;

	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		copy = g_ticks;
	}

	return copy;
}

static uint32_t getDelta()
{
	uint32_t copy;

	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		copy = g_delta;
	}

	return copy;
}

void motion_loadSequence()
{
	mem_init();
	for(uint16_t i = 0; i < mem_config.num_keyframes; ++i)
		mem_readKeyframe(i, &g_buffer[i]);
}

void motion_writeToBuffer(uint8_t index, const proto::Keyframe& kf)
{
	if(index >= proto::MAX_KEYFRAMES)
		return;

	g_buffer[index] = kf;
}

void motion_commit()
{
	for(uint16_t i = 0; i < mem_config.num_keyframes; ++i)
		mem_saveKeyframe(i, g_buffer[i]);
	mem_saveConfig();
}

static void executeOutputCommand(uint8_t cmd)
{
	switch(cmd)
	{
		case proto::OC_SET:
			io_setOutput(true);
			break;
		case proto::OC_RESET:
			io_setOutput(false);
			break;
	}
}

bool motion_keyframeReached(const proto::Keyframe& keyframe)
{
	int16_t max_diff = -1;
	for(uint8_t j = 0; j < mem_config.active_axes; ++j)
	{
		int16_t enc;
		if(!nt_encoderPosition(j+1, &enc))
			continue;

		int16_t diff = abs(((int16_t)keyframe.ticks[j]) - proto::NT_POSITION_BIAS - enc);

		if(diff > max_diff)
			max_diff = diff;
	}

	return max_diff >= 0 && max_diff < 50;
}

bool motion_isInStartPosition()
{
	proto::Keyframe start;
	mem_readKeyframe(0, &start);

	bool reached = motion_keyframeReached(start);

	if(reached)
		executeOutputCommand(start.output_command);

	return reached;
}

bool motion_doStartKeyframe()
{
	const proto::Keyframe& start = g_buffer[0];

	startTimer();
	resetTimer(8000); // ms

	uint8_t safety_counter = 0;

	while(!g_reached && !g_shouldStop)
	{
		for(uint8_t j = 0; j < mem_config.active_axes; ++j)
		{
			int32_t velocity = ((int32_t)mem_config.enc_to_mot[j]) * 94 / 256;
			nt_setVelocity(j+1, velocity);
			nt_setDestination(j+1, start.ticks[j]);

			// Get feedback for PC display
			nt_encoderPosition(j+1, &g_encPos[j]);
		}

		if(motion_keyframeReached(start))
		{
			if(++safety_counter == 10)
			{
				stopTimer();

				executeOutputCommand(start.output_command);

				return true;
			}
		}
		else
			safety_counter = 0;

		while(com_buf_to_bot.available())
		{
			// Break as soon as a command was received. Otherwise
			// the PC might be able to lock us in this loop.
			if(cmd_input(com_buf_to_bot.get()))
				break;
		}
	}

	// We did not reach the target position in 8s, better switch off power
	for(uint8_t j = 0; j < mem_config.active_axes; ++j)
		nt_setVelocity(j+1, 0);

	stopTimer();
	return false;
}

void motion_runSequence(bool force_loop)
{
	if(MOTION_PLOT)
	{
		printf("Playing sequence with %d keyframes on %d axes\n",
			mem_config.num_keyframes, mem_config.active_axes
		);
		printf("enc_to_mot is %u\n", mem_config.enc_to_mot[0]);
		printf("lookahead is %u\n", mem_config.lookahead);
	}

	g_shouldStop = false;
	g_isPlaying = true;

	if(!motion_isInStartPosition())
		g_shouldStop = !motion_doStartKeyframe();

	// If the user already aborted the operation, stop now
	if(g_shouldStop)
	{
		g_isPlaying = false;
		return;
	}

	executeOutputCommand(g_buffer[0].output_command);
	startTimer();

	int32_t speeds[proto::NUM_AXES];

	bool loop;

	do
	{
		loop = force_loop;

		for(uint8_t i = 1; i < mem_config.num_keyframes; ++i)
		{
			const proto::Keyframe& old = g_buffer[i-1];
			const proto::Keyframe& current = g_buffer[i];
			const proto::Keyframe* next = &g_buffer[i+1];

			if(i == mem_config.num_keyframes-1)
				next = &g_buffer[1];

			if(i == mem_config.num_keyframes-1 && !loop)
				break;

			// Fallback speed is calculated just based on the keyframe duration.
			// This is used when we cannot get encoder feedback.
			for(uint8_t j = 0; j < mem_config.active_axes; ++j)
			{
				uint16_t diff = abs(current.ticks[j] - old.ticks[j]);
				uint32_t enc_speed = 1000L * diff / current.duration;
				speeds[j] = mem_config.enc_to_mot[j] * enc_speed / 256;
			}

			resetTimer(current.duration);

			while(!g_reached && !g_shouldStop) // safe, byte access is atomic
			{
				if(MOTION_PLOT)
					printf("%6lu ", getTicks());

				for(uint8_t j = 0; j < mem_config.active_axes; ++j)
				{
					int32_t delta = getDelta() + mem_config.lookahead;
					if(g_reached)
						break;

					int16_t encPos;

					int32_t from = ((int32_t)old.ticks[j]) - proto::NT_POSITION_BIAS;
					int32_t to = ((int32_t)current.ticks[j]) - proto::NT_POSITION_BIAS;
					int32_t duration = current.duration;

					const proto::Keyframe* c = &current;
					uint16_t k = i;
					while(delta > c->duration)
					{
						if(k == mem_config.num_keyframes-2)
						{
							// Current keyframe is the last one.
							// If we are looping, choose next keyframe as 'next'
							// If not, just use current keyframe.

							if(io_button() || force_loop)
								loop = true;
							else
							{
								from = to;
								duration = 100;
								break;
							}
						}

						delta -= c->duration;

						if(k == mem_config.num_keyframes-1)
						{
							k = 1;
							c = &g_buffer[1];
						}
						else
						{
							++k;
							++c;
						}

						from = to;

						to = ((int32_t)c->ticks[j]) - proto::NT_POSITION_BIAS;
						duration = c->duration;
					}

					int32_t orig_vel = 0;
					int32_t dest = 0;
					int32_t vel = 0;

					// Velocity without adaption
					orig_vel = 1000L * (to - from) / duration;

					if(mem_config.lookahead && nt_encoderPosition(j+1, &encPos))
					{
						const int32_t maxSpeed = ((uint32_t)mem_config.enc_to_mot[j]) * 7000 / 256;

						// Calculate dest position
						dest = from + delta * orig_vel / 1000;

						// I want to be at 'dest' in LOOKAHEAD ms. Calculate needed velocity.
						vel = abs(1000L * (dest - encPos) / mem_config.lookahead);
						vel = vel * mem_config.enc_to_mot[j] / 256;

						speeds[j] = vel;

						// Never stop completely
						if(speeds[j] < 100)
							speeds[j] = 100;
						else if(speeds[j] > maxSpeed)
							speeds[j] = maxSpeed;

						nt_setDestination(j+1, dest+proto::NT_POSITION_BIAS);
						nt_setVelocity(j+1, speeds[j]);

						g_encPos[j] = encPos;
					}
					else if(mem_config.lookahead == 0)
					{
						// No velocity control wanted
						nt_setDestination(j+1, to+proto::NT_POSITION_BIAS);
						speeds[j] = abs(orig_vel);
						nt_setVelocity(j+1, speeds[j]);
					}

					if(MOTION_PLOT && j == 2)
						printf("%4ld %4lu %4d %4ld %4ld %4ld %4ld", to, speeds[j], encPos, from, dest, vel, duration);
				}

				if(MOTION_PLOT)
					printf("\n");

				while(com_buf_to_bot.available())
				{
					// Break as soon as a command was received. Otherwise
					// the PC might be able to lock us in this loop.
					if(cmd_input(com_buf_to_bot.get()))
						break;
				}
			}

			if(g_shouldStop)
				break;

			executeOutputCommand(current.output_command);
		}

		if(SYNCHRONIZE && loop && !force_loop) // do not wait if loop was commanded from PC
		{
			// We are done, wait for synchronization
			io_synchronize();
		}
	}
	while(loop && !g_shouldStop);

	if(PLOT_STOP)
	{
		resetTimer(20000);

		while(!g_reached)
		{
			int16_t encPos;
			int16_t cmd;
			nt_encoderPosition(4, &encPos);
			nt_command(4, &cmd);
			printf("%6lu %4d %4lu %4d %4d %4d %4ld\n", getTicks(), 0, speeds[3], encPos, cmd, 0, speeds[3]);
		}
	}

	stopTimer();

	g_isPlaying = false;
}

void motion_stop()
{
	g_shouldStop = true;
}

bool motion_isPlaying()
{
	return g_isPlaying;
}

int16_t motion_feedback(uint8_t motor_index)
{
	if(motor_index >= proto::NUM_AXES)
		return 0;

	if(g_isPlaying)
		return g_encPos[motor_index];
	else
	{
		int16_t ret = 0;
		if(nt_encoderPosition(motor_index+1, &ret))
			return ret;
		else
			return 0;
	}
}

void motion_executeSingleMotion(const proto::Motion& motion)
{
	for(uint8_t i = 0; i < motion.num_axes; ++i)
	{
		nt_setDestination(i+1, motion.ticks[i]);
		nt_setVelocity(i+1, motion.velocity[i]);
	}

	executeOutputCommand(motion.output_command);
}



