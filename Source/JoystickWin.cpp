#include "Joystick.h"
#include <windows.h>
#include <mmsystem.h>
#include <QDebug>

Joystick::Joystick()
{
	connected = false;

	for (int i = 0; i < numOfButtons; i++)
		button << false;

	axis << 0 << 0 << 0 << 0;
}

Joystick::~Joystick()
{
}

// Initializes the joystick with the first joystick found.
// Returns true on success and false if no joystick was found.
bool Joystick::init()
{
	JOYINFOEX joyInfoEx;
	ZeroMemory(&joyInfoEx, sizeof(joyInfoEx));
	joyInfoEx.dwSize = sizeof(joyInfoEx);
	bool joy1Present = (joyGetPosEx(JOYSTICKID1, &joyInfoEx) == JOYERR_NOERROR);
	connected = joy1Present;
	return joy1Present;
}

// Polls the joystick state. Returns true on success and false on error (e.g. the joystick was disconnected).
bool Joystick::update()
{
	JOYINFOEX joyInfoEx;
	joyInfoEx.dwSize = sizeof(joyInfoEx);
	joyInfoEx.dwFlags = JOY_RETURNALL;
	connected = (joyGetPosEx(JOYSTICKID1, &joyInfoEx) == JOYERR_NOERROR);
	if (!connected)
		return false;

	for (int i = 0; i < numOfButtons; i++)
	{
		buttonBefore[i] = button[i];
		button[i] = joyInfoEx.dwButtons & (1 << i);

		buttonPressed[i] = !buttonBefore[i] && button[i];
		buttonReleased[i] = buttonBefore[i] && !button[i];
	}

	axis[0] = (double)joyInfoEx.dwXpos/32767.5 - 1.0;
	axis[1] = (double)joyInfoEx.dwYpos/32767.5 - 1.0;
	axis[2] = (double)joyInfoEx.dwZpos/32767.5 - 1.0;
	axis[3] = (double)joyInfoEx.dwRpos/32767.5 - 1.0;

	return true;
}
