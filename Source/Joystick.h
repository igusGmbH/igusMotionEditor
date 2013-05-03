#ifndef Joystick_H
#define Joystick_H

/**
 * A joystick interface class.
 * Call init() to connect with the joystick.
 * Call update() to poll the joystick state.
 * The bool button[] array contains the button info (pressed or not)
 * and the double axis[] array contain axis info in the range [-1:1].
 */

#include <QList>

class Joystick
{
public:

	static const int numOfAxes = 4; /**< Number of supported axes. */
	static const int numOfButtons = 32; /**< Number of supported buttons. */

	bool connected;
	QList<bool> button;
	bool buttonPressed[numOfButtons];
	bool buttonReleased[numOfButtons];
	QList<double> axis;

	Joystick();
	~Joystick();

	// Initialize the joystick connection.
	bool init();

	// Poll the joystick state.
	bool update();

private:
	int jd;
	bool buttonBefore[numOfButtons];
};

#endif //Joystick_H
