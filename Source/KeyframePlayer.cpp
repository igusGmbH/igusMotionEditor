/*
 * KeyframePlayer.cpp
 *
 * This class is playing the current keyframes in the motion sequence.
 * When the play (or loop) button is pressed, the keyframe sequence is
 * converted to a linked list data structure. For each item in the list,
 * a point in time is calculated where the keyframe should be reached.
 * The actual playing is happening by advancing the "sliderPositon" in
 * real time. It's always the "next" keyframe that we are moving into
 * what is sent as a target position. Additionally, appropriate joint
 * velocities are calculated so that the robot would reach the keyframe
 * just at the right time. The velocity calculation is somewhat adaptive,
 * so that it can make up for little disturbances on the way.
 *
 *  Author: Marcell Missura, missura@ais.uni-bonn.de
 */

#include <QString>
#include <QHash>
#include <QList>
#include <QPointer>
#include <QtDebug>
#include <QHashIterator>
#include "KeyframePlayer.h"
#include "KeyframePlayerItem.h"
#include "Keyframe.h"
#include "globals.h"

KeyframePlayer::KeyframePlayer()
{
	timeCorrection = 0.08;
	velocityAdaptionStrength = 0.15;

	sliderPosition = 0.0;
	head = new KeyframePlayerItem();
	current = head;
	speedLimit = SERVOSPEEDMAX;
	looped = false;
	interpolating = false;
	velocityAdaption = true;

	// High precision tick counters.
	QueryPerformanceFrequency(&ticksPerSecond);
	QueryPerformanceCounter(&startTime);
	QueryPerformanceCounter(&lastTime);

	connect(&timer, SIGNAL(timeout()), this, SLOT(step()));
}

KeyframePlayer::~KeyframePlayer()
{
	delete head;
}

// Sets the maximum possible joint velocity.
void KeyframePlayer::setSpeedLimit(int sl)
{
	speedLimit = 0.01 * (double)sl * SERVOSPEEDMAX;
}

// Changes the time correction strength. This defines how much
// adaptation is allowed to cope with disturbances. Setting this
// value too high can and will cause oscillations.
void KeyframePlayer::setTimeCorrection(int sl)
{
	timeCorrection = 0.01 * (double)sl * 0.5;
}

/*
 * Stops the playing of keyframes.
 * Nothing happens if the player is not playing.
 */
void KeyframePlayer::stop()
{
	timer.stop();
}

/*
 * Tells you if the player is playing or not.
 */
bool KeyframePlayer::isPlaying()
{
	return timer.isActive();
}


/*
 * Updates the internal copy of joint angles currently received from the robot.
 * Using this feedback, factors are calculated that adapt the motion speed of the
 * joints during playing to correct errors.
 */
void KeyframePlayer::jointAnglesIn(QHash<QString, double> ja)
{
	rxJointAngles = ja;

	if (current->next && velocityAdaption)
	{
        foreach (QString key, head->joints.keys())
		{
			double deltaS = qAbs(txJointAngles[key] - rxJointAngles[key]);
			double deltaT = current->next->absoluteTime - sliderPosition + timeCorrection;
			txJointVelocityCorrectionFactors[key] = qBound(1.0 - velocityAdaptionStrength, deltaS / (deltaT * txJointVelocities[key]), 1.0 + velocityAdaptionStrength);
		}
	}
	else
	{
        foreach (QString key, head->joints.keys())
			txJointVelocityCorrectionFactors[key] = 1.0;
	}
}


/*
 * Loads a list of keyframes into the keyframe player.
 * It prepares a linked list of keyframe player items that is iterated when playing.
 */
void KeyframePlayer::playTheseFrames(QList< QPointer<Keyframe> > keyframes)
{
	if (keyframes.size() < 2)
		return;

	sliderPosition = 0.0;

	// Initialize the joint angles with the first frame.
	txJointAngles = keyframes[0]->jointAngles;

	// Initialize the velocities.
    foreach (QString key, head->joints.keys())
		txJointVelocities[key] = speedLimit;

	// Reset the correction factors.
    foreach (QString key, head->joints.keys())
		txJointVelocityCorrectionFactors[key] = 1.0;

	// Delete the old motion data structure.
	delete head->next;

	// Initialize the head.
	head->next = 0;
    head->setJointAngles(keyframes[0]->jointAngles);
	head->relativeTime = 0;
	head->absoluteTime = 0;
    head->outputCommand = keyframes[0]->getOutputCommand();
	current = head;

	// Build up the keyframe timeline.
	KeyframePlayerItem* item;
    for (int i = 0; i < keyframes.size(); i++)
	{
		double pause = (double)keyframes[i]->getPause();

		if (pause > 0)
		{
			item = new KeyframePlayerItem();
            item->setJointAngles(keyframes[i]->jointAngles);
			item->relativeTime = pause;
            item->absoluteTime = current->absoluteTime + item->relativeTime;
			current->next = item;
			current = item;
		}

        if(i == keyframes.size()-1)
            break; // No next keyframe to move to

		// Calculate the time it takes to reach the next keyframe.
		double keyframeDistance = qAbs(keyframes[i]->distance(keyframes[i+1]));
		double time = keyframeDistance / (0.01 * (double)keyframes[i+1]->getSpeed() * speedLimit);
		//qDebug() << "kfd:" << keyframeDistance << "sp:" << (0.01 * (double)keyframes[i+1]->getSpeed() * speedLimit) << "time:" << time;

		// Instantiate a new item and add it to the playlist.
		item = new KeyframePlayerItem();
        item->setJointAngles(keyframes[i+1]->jointAngles);
		item->relativeTime = time;
		item->absoluteTime = current->absoluteTime + item->relativeTime;
        item->outputCommand = keyframes[i+1]->getOutputCommand();
		current->next = item;
		current = item;
	}

	// If the motion is looped, the last keyframe has to be connected with the first one.
	if (looped)
	{
		// Calculate the time it takes to reach the next keyframe.
		double keyframeDistance = qAbs(keyframes.last()->distance(keyframes[0]));
		double time = keyframeDistance / (0.01 * (double)keyframes[0]->getSpeed() * speedLimit);

		// Instantiate a new item and add it to the playlist.
		item = new KeyframePlayerItem();
        item->setJointAngles(keyframes[0]->jointAngles);
		item->relativeTime = time;
		item->absoluteTime = current->absoluteTime + item->relativeTime;
        item->outputCommand = keyframes[0]->getOutputCommand();
		current->next = item;
		current = item;
	}

    // Calculate needed velocities
    current = head;
    while(current)
    {
        KeyframePlayerItem* next = current->next;

        if(!next)
            break;

        QHash<QString, KeyframePlayerItem::AxisInfo>::iterator it;
        for(it = current->joints.begin(); it != current->joints.end(); ++it)
        {
            double jointDistance = next->joints[it.key()].angle - it.value().angle;
            it.value().velocity = (jointDistance == 0 || next->relativeTime == 0) ?
                speedLimit : qAbs(jointDistance / next->relativeTime);
        }

        current = next;

        if(current == head)
            break;
    }

	// Reset the current pointer for playing.
	current = head;
}

void KeyframePlayer::start()
{
    // Start the timer.
    QueryPerformanceCounter(&lastTime);
    timer.start((int)(1000.0 / MOTIONSAMPLERATE));
}


/*
 * The main control of the keyframe player.
 * This is where the keyframe playing is happening.
 */
void KeyframePlayer::step()
{
	// Advance the slider position by the time passed since the last iteration.
	QueryPerformanceCounter(&tick);
	double timePassed = ((double)tick.QuadPart - (double)lastTime.QuadPart) / (double)ticksPerSecond.QuadPart;
	lastTime = tick;
	//sliderPosition += 1.0/MOTIONSAMPLERATE;
	sliderPosition += timePassed;

	// Check if the next keyframe has been overshot and advance the "current" pointer if needed.
	// The while loop covers the case when multiple keyframes have been stepped over in the last tick.
	while (current->next && current->next->absoluteTime < sliderPosition)
		current = current->next;

	// Check if the end of the motion sequence has been reached.
	if (!current->next)
	{
        txJointAngles = current->jointAngles();

        foreach (QString key, head->joints.keys())
			txJointVelocities[key] = speedLimit;

		if (looped)
		{
			sliderPosition = sliderPosition - current->absoluteTime;
			current = head;
		}
		else
		{
			stop();
			emit motionOut(txJointAngles, txJointVelocities);
			emit finished();
			return;
		}
	}
	else
	{
		// Calculate new target positions.
		foreach (QString key, txJointAngles.keys())
		{
            double jointDistance = current->next->joints[key].angle - current->joints[key].angle;
			if (interpolating)
                txJointAngles[key] = current->joints[key].angle + jointDistance * qMin(1.0, (sliderPosition - current->absoluteTime) / current->next->relativeTime);
			else
                txJointAngles[key] = current->next->joints[key].angle;
            txJointVelocities[key] = (jointDistance == 0 || current->next->relativeTime == 0) ?
                        speedLimit : txJointVelocityCorrectionFactors[key] * qAbs(jointDistance / (current->next->relativeTime + timeCorrection));
		}
	}

	emit motionOut(txJointAngles, txJointVelocities);
}

/**
 * @warning Returned pointer is only valid until next playTheseFrames() call!
 */
const KeyframePlayerItem* KeyframePlayer::playingList()
{
    return head;
}

