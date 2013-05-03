// Keyframe memory
// Author: Max Schwarz <max.schwarz@uni-bonn.de>

#include "mem.h"

#include <avr/eeprom.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include <stdio.h>

proto::Keyframe __attribute__((section(".eeprom"))) g_keyframe_memory[proto::MAX_KEYFRAMES]
 = {{0, {0x00}}};
proto::Config __attribute__((section(".eeprom"))) g_config_memory
 = {0xF, 0};

proto::Config mem_config;

bool g_readRequest = true;
uint8_t g_readIdx = 0;
uint8_t g_readOffset = 0;

void mem_init()
{
	bool invalid = false;

	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		eeprom_read_block(&mem_config, &g_config_memory, sizeof(proto::Config));

		// Validity check
		if(mem_config.active_axes == 0xFFFF || mem_config.num_keyframes >= proto::MAX_KEYFRAMES)
		{
			invalid = true;

			mem_config.active_axes = 4;
			mem_config.num_keyframes = 0;
		}
	}

	if(invalid)
		printf("No valid configuration found in EEPROM\n");
}

void mem_readKeyframe(uint8_t index, proto::Keyframe* dest)
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		eeprom_read_block(dest, g_keyframe_memory + index, sizeof(proto::Keyframe));
	}
}

void mem_saveKeyframe(uint8_t index, const proto::Keyframe& src)
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		eeprom_update_block(&src, g_keyframe_memory + index, sizeof(proto::Keyframe));
	}
}

void mem_saveConfig()
{
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
	{
		eeprom_update_block(&mem_config, &g_config_memory, sizeof(proto::Config));
	}
}
