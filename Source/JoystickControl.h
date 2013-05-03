#ifndef JoystickControl_H
#define JoystickControl_H

/**
 * A joystick control class.
 */

#include <QObject>
#include <QHash>
#include <QTimer>
#include "Joystick.h"
#include "JointConfiguration.h"

class JoystickControl : public QObject
{
	Q_OBJECT

	Joystick joystick;
	bool connected;
	double speedLimit;
	QHash<QString, double> rxJointAngles;
	QTimer timer;

	QHash<QString, double> txxJointAngles;
    JointInfo::ListPtr m_jointConfig;

public:
	JoystickControl();
	~JoystickControl(){};

public slots:
	void update();
	void jointAnglesIn(QHash<QString, double>);
	void setSpeedLimit(int sl);
    void setJointConfig(const JointInfo::ListPtr& config);

signals:
	void joystickConnected();
	void joystickDisconnected();
	void message(QString);
	void buttonPressed(QList<bool> button);
	void motionOut(QHash<QString, double>, QHash<QString, double>);
	void joystickOut(QHash<QString, double>);

};

#endif //JoystickControl_H
