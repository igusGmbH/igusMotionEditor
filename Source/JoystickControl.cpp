#include "JoystickControl.h"
#include "globals.h"
#include <QDebug>

/*
 * The joystick control object uses the joystick input to generate a
 * motionOut() signal as input for the keyframe editor (not the robot
 * interface directly). The idea is that moving the joystick should
 * be essentially the same as moving the sliders with the mouse.
 * The produced motion signal is generated from the received poses from
 * the robot by adding a small target position and defining a velocity
 * limit that both depend on how far the joystick is moved.
 */

JoystickControl::JoystickControl()
{
	// The joystick control is running periodically triggered by an
	// internal timer. The generated motion trajectory is emitted as
	// a Qt signal.

	connected = false;
	speedLimit = 0;
	joystick.init();
	connect(&timer, SIGNAL(timeout()), this, SLOT(update()));
	timer.start(1000.0/JOYSTICKRATE);
}

void JoystickControl::update()
{
	bool isJoyConnected = joystick.update();

	// Handle connect event.
	if (!connected && isJoyConnected)
	{
		connected = true;
		emit message("Joystick connected.");
		emit joystickConnected();
	}

	// Handle disconnect event.
	else if (connected && !isJoyConnected)
	{
		connected = false;
		emit message("Joystick disconnected.");
		emit joystickDisconnected();
	}

	// Do the usual joystick business when connected.
	if (connected)
	{
		QHash<QString, double> txJointVelocity;
		QHash<QString, double> txJointAngles;
		QHash<QString, double> txJoystick;

		// The motion signal is generated using the "minimal carrot" algorithm.
		// You know how you hold a carrot in front of a donkey so that it starts
		// moving? The distance of the carrot should be as small as possible, so
		// that the donkey stops at the carrot if the connection breaks and the
		// carrot could not be updated. But the carrot distance should be large
		// enough so that the donkey cannot reach it in one iteration.

		double joystickThreshold = 0.25;
		double carrot = speedLimit * 1.0/JOYSTICKRATE + 0.25;
		bool somethingIsNonZero = false;

        foreach(const JointInfo& joint, *m_jointConfig)
        {
            int axis = joint.joystick_axis;

            if(axis < 0)
                continue;

            if(axis >= joystick.numOfAxes)
                continue;

            int js_value = joystick.axis[axis];
            if(joint.joystick_invert)
                js_value = -js_value;

            if(qAbs(js_value) > joystickThreshold)
            {
                txJointAngles[joint.name] = rxJointAngles[joint.name] + js_value * carrot;
                txJointVelocity[joint.name] = qAbs(js_value) * speedLimit;
                txJoystick[joint.name] = js_value;

                somethingIsNonZero = true;
            }
        }

		if (somethingIsNonZero)
		{
			emit motionOut(txJointAngles, txJointVelocity);
			emit joystickOut(txJoystick);
		}

		for (int i = 0; i < Joystick::numOfButtons; i++)
		{
			if (joystick.buttonPressed[i])
			{
				emit buttonPressed(joystick.button);
				break;
			}
		}
	}

	return;
}


/*
 * Updates the internal copy of joint angles.
 * The joystick control receives a constant stream of joint angles from the robot interface.
 */
void JoystickControl::jointAnglesIn(QHash<QString, double> ja)
{
	rxJointAngles = ja;
}

/*
 * Sets the speed limit.
 */
void JoystickControl::setSpeedLimit(int sl)
{
	speedLimit = 0.01 * (double)sl * SERVOSPEEDMAX;
}

void JoystickControl::setJointConfig(const JointInfo::ListPtr &config)
{
    m_jointConfig = config;
}
