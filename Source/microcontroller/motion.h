// Motion control
// Author: Max Schwarz <max.schwarz@uni-bonn.de>

#ifndef MOTION_H
#define MOTION_H

#include "protocol.h"

/**
 * Run the motion sequence. Motion automatically loops as long as
 * io_button() is pressed.
 *
 * @param force_loop Loop even if io_button() is not pressed.
 *        This also disables the synchronization.
 **/
void motion_runSequence(bool force_loop = false);

void motion_loadSequence();

void motion_writeToBuffer(uint8_t index, const proto::Keyframe& kf);
void motion_commit();

bool motion_isInStartPosition();

int16_t motion_feedback(uint8_t motor_index);

/**
 * Move to the first keyframe.
 **/
bool motion_doStartKeyframe();

void motion_stop();

bool motion_isPlaying();

/**
 * Execute single motion with specified velocity
 **/
void motion_executeSingleMotion(const proto::Motion& motion);

#endif
