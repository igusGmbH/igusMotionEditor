/*
 * RobotInterface.h
 *
 *  Created on: Jan 16, 2009
 *  Author: Marcell Missura, missura@ais.uni-bonn.de
 */

#ifndef ROBOTINTERFACE_H_
#define ROBOTINTERFACE_H_

#include <QHash>
#include <QString>
#include <QObject>
#include <QTime>
#include <QTimer>
#include <QMutex>
#include <QThread>
#include <QFile>
#include <QTextStream>
#include <QPointer>
#include "Serial.h"
#include "Keyframe.h"
#include "microcontroller/protocol.h"

class KeyframePlayerItem;

class RobotInterface : public QThread
{
	Q_OBJECT

public:
    enum ComplianceMode
	{
        hardwareCompliance,
		noCompliance,
	};

    enum KeyframeCommand
    {
        KC_COMMIT,
        KC_PLAY,
        KC_LOOP
    };

private:

	static const int TIMEOUT = 10; // How many times do you try to receive a packet before you give up.
    static const int PORTCYCLE = 15; // How many ports to cycle when searching for a robot.
    static const qint64 BUFFER_SIZE = 64; // The size of the receive buffer.

	ComplianceMode complianceMode;
    ComplianceMode requestedComplianceMode;

    struct MotorData
    {
        JointInfo joint;
        bool isReset;
        bool isInitialized;
        bool isHWCompliant;
    };

    QHash<QString, MotorData> m_motors;
    QMutex m_bigMotorMutex; // Locks m_motors structure

	bool robotIsConnected;
    bool robotIsReset;
	bool doInitialize;
    bool robotIsInitialized;
	bool doCheckInitialization;

    bool m_isExtendedMode;
    bool m_isPlaying;
    bool m_stopPlaying;

	int timeoutTicksLeft;
	char receiveBuffer[BUFFER_SIZE];
    int portNumber;
	CSerial serial;

	int encoderPosition;
	int motorPosition;
	int registerValue;

	double speedLimit;
    int m_lookahead;

	QHash<QString, double> txJointAngles;
    QHash<QString, double> txJointVelocities;
	QHash<QString, double> rxJointAngles;
    QHash<QString, double> rxJointVelocities;
    QHash<QString, double> lastRxJointAngles;
    int txOutputCommand;

    // Log and precise timer for debugging.
	LARGE_INTEGER startTime;
    LARGE_INTEGER lastTime;
    LARGE_INTEGER ticksPerSecond;
    LARGE_INTEGER tick;
    QFile logfile;
    QTextStream log;

    int m_noFeedbackCounter;
public:

	RobotInterface();
	virtual ~RobotInterface();

	bool isRobotInitialized();
    bool isRobotConnected();
	void stop();
    void stopPlaying();
    bool isPlaying();

public slots:
	void motionIn(QHash<QString, double>, QHash<QString, double>);
    void motionIn(QHash<QString, double>, QHash<QString, double>, int outputCommand);
	void setSpeedLimit(int sl);
	void initializeRobot();
    void step();
    void setJointConfig(const JointInfo::ListPtr& config);
    void setComplianceMode(int mode);
    void stopRobot();
    void transferKeyframes(const KeyframePlayerItem* head, int cmd);

signals:
	void robotConnected();
	void robotInitialized();
	void robotDisconnected();
    void robotConnectionChanged(bool connected);
	void message(QString);
	void limitsLoaded(QHash<QString, double>, QHash<QString, double>);
	void motionOut(QHash<QString, double>, QHash<QString, double>);
    void playbackStarted();
    void playbackFinished();
    void complianceChanged(int mode);
    void keyframeTransferFinished(bool success);

protected:
	void run();

private:
	void setPortNumber(int pn);
	void disconnectRobot();
	QString txrx(QString command);
	bool pollRegister(QString reg);
	bool pollPiggyBackRegister(QString reg);

    void flashDone(bool success);

    void handle_checkComplianceMode();
    void handle_confirmConnection();
    void handle_robotReset();
    void handle_checkInitialization();
    void handle_initialize();
    void handle_doHardwareComplianceMode();
    void handle_undoHardwareComplianceMode();
    void handle_extendedMode();
    void handle_flashRequest();

    // Extended mode communication helpers
    template<class Cmd, class Answer>
    bool extCommand(const Cmd& cmd, Answer* answer);

    template<class Cmd, class Answer>
    bool extChat(const Cmd& cmd, const Answer& expectedReply);

    bool extDisable();
    bool extEnable();

    bool extSendConfig(int num_frames);
};

#endif /* ROBOTINTERFACE_H_ */
