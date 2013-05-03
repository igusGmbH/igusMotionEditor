// Command handling
// Author: Max Schwarz <max.schwarz@uni-bonn.de>

#ifndef COMMANDS_H
#define COMMANDS_H

#include <stdint.h>

void handleCommand(uint8_t command, const uint8_t* payload, uint8_t length);
void handleCommands();

/**
 * Process single input char.
 *
 * @return true if a command was complete
 **/
bool cmd_input(uint8_t c);

bool cmd_extEnabled();

#endif
