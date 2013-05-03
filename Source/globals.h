#ifndef GLOBS_H_
#define GLOBS_H_

#define	sgn(a)( (a) == 0 ? 0 : (((a) < 0) ? -1 : 1) )

#define G 9.81
#define PI 3.1415926535897932384626433832795
#define PI2 1.5707963267948965579989817342721
#define SPI 1.7724538509055160272981674833411
#define EPSILON 0.0001


#define SERIALCOMRATE 16.39 // Frequency of the serial communication loop in Hz.
#define JOYSTICKRATE 25.0 // Frequency of the joystick stream.
#define MOTIONSAMPLERATE 50.0 // Frequency of the motion sampling of the keyframe player.
#define SERVOSPEEDMAX 4.0 // Maximum speed of the servos in rad per second.

#endif /* GLOBS_H_ */
