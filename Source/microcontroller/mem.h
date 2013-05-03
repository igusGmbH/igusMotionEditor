// Keyframe memory
// Author: Max Schwarz <max.schwarz@uni-bonn.de>

#ifndef MEM_H
#define MEM_H

#include <stdint.h>

#include "protocol.h"

void mem_readKeyframe(uint8_t index, proto::Keyframe* dest);
void mem_saveKeyframe(uint8_t index, const proto::Keyframe& src);

extern proto::Config mem_config;

void mem_init();
void mem_saveConfig();

#endif
