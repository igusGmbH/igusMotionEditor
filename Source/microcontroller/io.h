// Digital I/O
// Author: Max Schwarz <max.schwarz@uni-bonn.de>

#ifndef IO_H
#define IO_H

void io_init();

//! Start button
bool io_button();

//! Digital output
void io_setOutput(bool active);

void io_synchronize();

#endif
