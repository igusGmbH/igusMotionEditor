/*
 * RobotInterface.cpp
 *
 * Takes care of the communication between the Motion Editor and the robot.
 * The terms "tx" and "rx" are frequently used for transmission and reception.
 *
 * For the communication with the robot the motionIn() slot and the motionOut() signal
 * are most important. motionIn() is the interface for other objects to stream motions
 * into the robot interface. motionOut() is periodically emitted to broadcast the currently
 * received robot pose to other objects.
 *
 * The interface automatically takes care of some features in the background without your
 * intervention. It automatically tries to connect to the robot through the serial port and to
 * detect which robot is plugged in so all you need to do is make sure that the cables are
 * plugged right and the software is ready to use. When the robot is detected, the appropriate
 * calibration file is loaded. In case the connection is lost, it automatically tries to recover.
 * If anything goes wrong, such as the robot is turned off or the connection is lost altogether,
 * or any of these things are regained after being lost, the interface sends out signals to
 * inform a higher layer of the event.
 *
 * The robot interface runs as a separate thread and tries to complete the communication loop
 * as fast as it can.
 *
 *  Author: Marcell Missura, missura@ais.uni-bonn.de
 */

#include <QHash>
#include <QHashIterator>
#include <QString>
#include <QFile>
#include <QTextStream>
#include <QtDebug>
#include <QByteArray>
#include "RobotInterface.h"
#include "Keyframe.h"
#include "globals.h"
#include "microcontroller/protocol.h"
#include "KeyframePlayerItem.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

/**
 * @file
 *
 * GENERAL DESIGN
 *
 * The RobotInterface uses its own thread to manage communication
 * as fast as possible. The step() method and all handle_* methods
 * live in this thread.
 *
 * To accelerate motion commands and feedback, they are combined
 * into a single µC packet for all joints in handle_extendedMode().
 *
 * TODO: Use proper QThread model. This means communication happens
 * only via signals/slots and the thread has its own event loop.
 * We can do away with mutexes and stuff like that.
 */

RobotInterface::RobotInterface()
 : m_isExtendedMode(false)
 , txOutputCommand(proto::OC_NOP)
{
	portNumber = 2; // The default port.
	timeoutTicksLeft = TIMEOUT;

    robotIsConnected = false;
    robotIsReset = true;
	robotIsInitialized = false;
	doInitialize = false;
	doCheckInitialization = true;

    requestedComplianceMode = complianceMode = noCompliance;
    m_isExtendedMode = false;
    m_isPlaying = false;

	// Log and high precision tick counters for debugging.
	QueryPerformanceFrequency(&ticksPerSecond);
	QueryPerformanceCounter(&startTime);
	QueryPerformanceCounter(&lastTime);
	logfile.setFileName("data.log");
	logfile.open(QFile::WriteOnly | QFile::Truncate);
	log.setDevice(&logfile);

	encoderPosition = 0;
    motorPosition = 0;

    m_noFeedbackCounter = 0;

    // Execute step() function as often as possible
    QTimer* timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), SLOT(step()));
    timer->start(0);

    // Slots should be executed in this thread
    moveToThread(this);

	// Attention, the robot interface is started in IgusMotionEditor at the end of the constructor
	// to make sure that the connections to and from the robot interface are already made.
}

RobotInterface::~RobotInterface()
{
    exit();
	wait();
	serial.close();
	logfile.close();
}

// Sets the speed limit for the joints.
// This is only used for the software compliance mode.
void RobotInterface::setSpeedLimit(int sl)
{
	speedLimit = 0.01 * (double)sl * SERVOSPEEDMAX;
}

/*
 * Sets the serial port identifier to something.
 * It closes the current port and it will try to
 * automatically reconnect to the new port.
 */
void RobotInterface::setPortNumber(int pn)
{
	portNumber = pn;
	serial.close();

	if (robotIsConnected)
		disconnectRobot();

	//emit message("Searching for a robot on " + port->portName() + ".");
}

/*
 * Sets the compliance mode to hardware, software or no compliance.
 * In hardware compliance mode the power to the motors is turned off, so that the robot can be moved by hand.
 * In software compliance mode the robot makes an attempt to move along when moved by hand.
 * In no compliance mode the robot is stiff and tries its best to hold the currently commanded position.
 */
void RobotInterface::setComplianceMode(int cm)
{
	if (!isRobotInitialized())
    {
        message("Please initialize the robot first.");
		return;
    }

    if(cm == requestedComplianceMode)
        return;

    requestedComplianceMode = (ComplianceMode)cm;
}

/*
 * Sets the joint angles and velocities that are sent to the robot from now on.
 * This function is guarding the joint angle and velocity limits by truncating values that are too high or too low.
 */
void RobotInterface::motionIn(QHash<QString, double> angles, QHash<QString, double> velocities, int outputCommand)
{
	QHashIterator<QString, double> i(rxJointAngles);
	while (i.hasNext())
	{
		i.next();
        const MotorData& m = m_motors[i.key()];
		if (angles.contains(i.key()))
            txJointAngles[i.key()] = qBound(
                m.joint.lower_limit,
                angles[i.key()],
                m.joint.upper_limit
            );
		if (velocities.contains(i.key()))
            txJointVelocities[i.key()] = qBound(
                0.0, qAbs(velocities[i.key()]), SERVOSPEEDMAX
            );
	}
    txOutputCommand = outputCommand;
}

void RobotInterface::motionIn(QHash<QString, double> angles, QHash<QString, double> velocities)
{
    motionIn(angles, velocities, proto::OC_NOP);
}

/*
 * A special function to stop the robot immediately in its current position.
 */
void RobotInterface::stopRobot()
{
	QHashIterator<QString, double> i(rxJointAngles);
	while (i.hasNext())
	{
		i.next();
		txJointAngles[i.key()] = rxJointAngles[i.key()];
		txJointVelocities[i.key()] = 0.0;
    }
}

/*
 * Tells you if the robot is connected to the com port.
 */
bool RobotInterface::isRobotConnected()
{
	return robotIsConnected;
}

/*
 * Does whatever needs to be done to disconnect the robot.
 */
void RobotInterface::disconnectRobot()
{
	if (complianceMode == hardwareCompliance)
	{
        complianceMode = noCompliance;
	}

    QHash<QString, MotorData>::iterator it;
    for(it = m_motors.begin(); it != m_motors.end(); ++it)
    {
        MotorData* m = &it.value();
        m->isReset = false;
        m->isInitialized = false;
        m->isHWCompliant = false;
    }

	robotIsConnected = false;
    robotIsReset = true;
	robotIsInitialized = false;
	doCheckInitialization = true;
    doInitialize = false;
    m_isExtendedMode = false;
    m_isPlaying = false;

    emit robotConnectionChanged(false);
	emit robotDisconnected();
	emit message("ROBOT lost!");
}

/*
 * Tells you if the robot is initialized.
 */
bool RobotInterface::isRobotInitialized()
{
	return robotIsInitialized;
}


/*
 * Triggers the initialization routine.
 */
void RobotInterface::initializeRobot()
{
	if (!robotIsConnected)
		return;

    complianceMode = noCompliance; // TODO: Check if this is true

    QHash<QString, MotorData>::iterator it;
    for(it = m_motors.begin(); it != m_motors.end(); ++it)
    {
        MotorData* m = &it.value();
        m->isReset = false;
        m->isInitialized = false;
    }

	robotIsReset = false;
	robotIsInitialized = false;
	doInitialize = true;
    doCheckInitialization = false;
	emit message("Initializing...");
}

/*
 * Stops the serial communication thread.
 */
void RobotInterface::stop()
{
    exit();
}

inline int kf_output_cmd_to_proto(int cmd)
{
    switch(cmd)
    {
        case Keyframe::DO_RESET:
            return proto::OC_RESET;
        case Keyframe::DO_SET:
            return proto::OC_SET;
        case Keyframe::DO_IGNORE:
            return proto::OC_NOP;
    }

    qDebug() << "WARNING: Unknown output command" << cmd;
    return proto::OC_NOP;
}

/**
 * Transfer keyframes to the microcontroller. Depending on the KeyframeCommand
 * different actions are taken (e.g. playback of the sequence, or save to
 * EEPROM)
 */
void RobotInterface::transferKeyframes(const KeyframePlayerItem *head, int cmd)
{
    QList<proto::Keyframe> frames;
    m_stopPlaying = false;

    log << "transferKeyframes\n";

    // Build the keyframe list
    // Push initial state (first keyframe)
    if(head)
    {
        proto::Keyframe init;
        init.duration = 0;
        init.output_command = kf_output_cmd_to_proto(head->outputCommand);

        QHash<QString, KeyframePlayerItem::AxisInfo>::const_iterator it;
        for(it = head->joints.begin(); it != head->joints.end(); ++it)
        {
            if(!m_motors.contains(it.key()))
                continue;

            const MotorData& m = m_motors[it.key()];

            int idx = m.joint.address - 1;
            double sgn = m.joint.invert ? -1 : 1;
            init.ticks[idx] = qRound((sgn * it.value().angle + m.joint.offset) / m.joint.enc_to_rad) + proto::NT_POSITION_BIAS;
        }

        frames.push_back(init);
    }

    for(; head; head = head->next)
    {
        QHash<QString, KeyframePlayerItem::AxisInfo>::const_iterator it;

        const KeyframePlayerItem* next = head->next;

        if(!next || next == head)
            break;

        proto::Keyframe cmd;

        cmd.duration = next->relativeTime * 1000;
        cmd.output_command = kf_output_cmd_to_proto(next->outputCommand);

        for(it = next->joints.begin(); it != next->joints.end(); ++it)
        {
            if(!m_motors.contains(it.key()))
                continue;

            const MotorData& m = m_motors[it.key()];

            int idx = m.joint.address - 1;
            double sgn = m.joint.invert ? -1 : 1;
            cmd.ticks[idx] = qRound((sgn * it.value().angle + m.joint.offset) / m.joint.enc_to_rad) + proto::NT_POSITION_BIAS;
        }

        frames.append(cmd);
    }


    foreach(const proto::Keyframe& kf, frames)
    {
        log << "KEYFRAME\n";
        log << "  duration: " << kf.duration << "output: " << kf.output_command << '\n';
        for(int i = 0; i < 4; ++i)
            log << i << " " << kf.ticks[i] << '\n';
    }

    // Everything prepared, begin flash process

    if(!extSendConfig(frames.length()))
    {
        emit keyframeTransferFinished(false);
        return;
    }

    for(int i = 0; i < frames.length(); ++i)
    {
        const proto::Keyframe keyframe = frames[i];
        proto::Packet<proto::CMD_SAVE_KEYFRAME, proto::SaveKeyframe> packet;

        packet.payload.index = i;
        packet.payload.keyframe = keyframe;

        packet.updateChecksum();
        if(!extChat(packet, proto::SimplePacket<proto::CMD_SAVE_KEYFRAME>()))
        {
            emit message(tr("Could not save keyframe %1").arg(i));
            emit keyframeTransferFinished(false);
            return;
        }
    }

    switch(cmd)
    {
        case KC_COMMIT:
            if(!extChat(proto::SimplePacket<proto::CMD_COMMIT>(), proto::SimplePacket<proto::CMD_COMMIT>()))
            {
                emit message(tr("Could not write to EEPROM"));
                emit keyframeTransferFinished(false);
                return;
            }
            break;
       case KC_PLAY:
       case KC_LOOP:
            proto::Packet<proto::CMD_PLAY, proto::Play> play;
            play.payload.flags = 0;
            if(cmd == KC_LOOP)
                play.payload.flags |= proto::PF_LOOP;

            play.updateChecksum();

            if(!extChat(play, proto::SimplePacket<proto::CMD_PLAY>()))
            {
                emit message(tr("Could not start playback"));
                emit keyframeTransferFinished(false);
                return;
            }

            emit playbackStarted();
            m_isPlaying = true;
            break;
    }

    emit keyframeTransferFinished(true);
}

bool RobotInterface::extSendConfig(int num_frames)
{
    // Find the number of axes
    int num_axes = 0;
    foreach(const MotorData& m, m_motors.values())
    {
        if(m.joint.address > num_axes)
            num_axes = m.joint.address;
    }

    if(num_axes > proto::NUM_AXES)
    {
        emit message("Number of joints is too big for microcontroller");
        return false;
    }

    proto::Packet<proto::CMD_CONFIG, proto::Config> configPacket;
    configPacket.payload.active_axes = num_axes;
    configPacket.payload.num_keyframes = num_frames; // TODO: error message if too large
    configPacket.payload.lookahead = m_lookahead;

    foreach(const MotorData& m, m_motors.values())
    {
        configPacket.payload.enc_to_mot[m.joint.address-1] = 256.0 * m.joint.enc_to_rad / m.joint.mot_to_rad;
        log << "enc_to_mot for " << m.joint.name << ": " << configPacket.payload.enc_to_mot[m.joint.address-1];
    }

    configPacket.updateChecksum();

    if(!extChat(configPacket, proto::SimplePacket<proto::CMD_CONFIG>()))
    {
        emit message("Could not write configuration");
        return false;
    }

    return true;
}

void RobotInterface::setJointConfig(const JointInfo::ListPtr &config)
{
    m_motors.clear();
    rxJointAngles.clear();

    foreach(const JointInfo& joint, *config)
    {
        MotorData m;
        m.joint = joint;
        m.isReset = false;
        m.isInitialized = false;
        m.isHWCompliant = false;

        m_motors[joint.name] = m;

        // Initialize joint angles
        rxJointAngles[joint.name] = 0.0;
    }

    txJointAngles = txJointVelocities = rxJointAngles;
    m_lookahead = config->lookahead;
}

bool RobotInterface::isPlaying()
{
    return m_isPlaying;
}

void RobotInterface::stopPlaying()
{
    m_stopPlaying = true;
}

///////////////////////////////////////////////////////////////////////////////
// START COMMUNICATION THREAD CODE
///////////////////////////////////////////////////////////////////////////////
//@{

/*
 * Handles one exchange of packets with one servo controllers.
 * It sends the command to the controller and waits for the answer.
 * The return value is what was last read from the socket.
 */
QString RobotInterface::txrx(QString command)
{
    int bytesWritten = serial.write((void*)command.toAscii().constData(), command.length());
    command.replace("\r", "\\r");
    //qDebug() << "Command:" << command.left(command.length()) << bytesWritten << " bytes sent";

    // This is how a disconnection is detected.
    if (bytesWritten != 0)
    {
        // Close the broken port.
        if (serial.isOpen())
        {
            emit message("Port COM" + QString::number(portNumber) + " disconnected.");
            serial.close();
        }

        if (robotIsConnected)
            disconnectRobot();

        return QString("PORTBROKEN");
    }

    // Overlapped communication with blocking wait.
    memset(receiveBuffer, 0, BUFFER_SIZE);
    serial.WaitEvent(200);
    int bytesRead = serial.read(receiveBuffer, BUFFER_SIZE);
    QString response = QString::fromAscii(receiveBuffer, bytesRead);
    response.replace("\r", "\\r");
    //qDebug() << "Response:" << bytesRead << response;

    log << "Plain cmd: '" << command << "' -> '" << response << '\n';

//    if(bytesRead >= 1)
//        qDebug() << "Dec:" << (int)response[response.length()-1].toAscii();

    if(response.endsWith("\\r"))
        response.chop(2);



    // Time out on too many failed read attempts.
    if (bytesRead == 0 && robotIsConnected)
    {
        timeoutTicksLeft--;

        if (timeoutTicksLeft == 0)
        {
            timeoutTicksLeft = TIMEOUT;
            qDebug() << "Timeout";
            disconnectRobot();
        }
    }
    else
    {
        timeoutTicksLeft = TIMEOUT;
    }

    return response;
}

bool RobotInterface::pollRegister(QString reg)
{
    bool ok = false;
    QString response = txrx("#" + reg + "\r");
    QRegExp rx = QRegExp(reg + "([+-]?\\d+)");
    if (rx.indexIn(response) != -1)
    {
        int ticks = rx.cap(1).toInt(&ok);
        if (ok)
            registerValue = ticks;
    }

    return ok;
}

bool RobotInterface::pollPiggyBackRegister(QString reg)
{
    bool ok = false;
    QString response = txrx("#" + reg + "\r");
    QRegExp rx = QRegExp(reg + "([+-]?\\d+)");
    if (rx.indexIn(response) != -1)
    {
        int ticks = rx.cap(1).toInt(&ok);
        if (ok)
        {
            encoderPosition = ((ticks & 0xFFF00000) >> 20) - proto::NT_POSITION_BIAS;
            motorPosition = (ticks & 0x000FFFFF) - 524288;
        }
    }

    return ok;
}

/**
 * Write an extended command to serial port and save the answer.
 *
 * Usage:
 *  proto::SimplePacket<proto::CMD_EXIT> cmd, response;
 *  extCommand(serial, cmd, &response);
 *
 * @return false if there was an error (e.g. checksum mismatch)
 */
template<class Cmd, class Answer>
bool RobotInterface::extCommand(const Cmd& cmd, Answer* answer)
{
    log << "Extended cmd:";
    for(size_t i = 0; i < sizeof(cmd); ++i)
        log << ' ' << QString::number(((uint8_t*)&cmd)[i], 16).rightJustified(2, '0');
    log << " -> ";

    unsigned char* buf = (unsigned char*)malloc(sizeof(Answer));
    unsigned char* readptr = buf;
    int remsize = sizeof(Answer);

    serial.write((void*)&cmd, sizeof(cmd));

    int counter = 0;

    while(remsize > 0)
    {
        if(++counter > 10)
        {
            log << "timeout\n";
            return false;
        }

        int ret = serial.read(readptr, remsize);
        if(ret == 0)
        {
            serial.WaitEvent(50);
            continue;
        }
        if(ret <= 0)
        {
            log << "read error " << ret << '\n';
            return false;
        }

        for(size_t i = 0; i < ret; ++i)
            log << ' ' << QString::number(((uint8_t*)readptr)[i], 16).rightJustified(2, '0');

        remsize -= ret;
        readptr += ret;

        // Adjust beginning
        if(readptr - buf < 4)
            continue;

        // We know the answer begins with the command header, so
        // look for it (in a stupid way, I know)
        int i = 0;
        while(memcmp(&cmd, buf, 3) != 0)
        {
            ++i;
            if(i == readptr - buf)
                break;
        }

        if(i != 0)
        {
            memmove(buf, buf + i, sizeof(Answer) - i);
            remsize += i;
            readptr -= i;
        }
    }

    memcpy(answer, buf, sizeof(Answer));
    log << "corrected:";
    for(size_t i = 0; i < sizeof(Answer); ++i)
        log << ' ' << QString::number(((uint8_t*)answer)[i], 16).rightJustified(2, '0');

    if(answer->currentChecksum() != answer->checksum)
    {
        log << "checksum mismatch, should be 0x" << QString::number(answer->currentChecksum(), 16) << '\n';
        return false;
    }

    log << '\n';

    return true;
}

/**
 * Write an extended command to serial port and expect a fixed answer.
 *
 * @return false if the answer did not match or another error occured.
 */
template<class Cmd, class Answer>
bool RobotInterface::extChat(const Cmd& cmd, const Answer& expectedReply)
{
    Answer answer;

    if(!extCommand(cmd, &answer))
        return false;

    return memcmp(&expectedReply, &answer, sizeof(Answer)) == 0;
}

/**
 * Disable extended mode.
 */
bool RobotInterface::extDisable()
{
    return extChat(proto::SimplePacket<proto::CMD_EXIT>(), proto::SimplePacket<proto::CMD_EXIT>());
}

/**
 * Enable extended mode
 */
bool RobotInterface::extEnable()
{
    return extChat(proto::SimplePacket<proto::CMD_INIT>(), proto::SimplePacket<proto::CMD_INIT>());
}

/**
 * Confirm the connection with a status query and load the calibration.
 */
void RobotInterface::handle_confirmConnection()
{
    // Send an CMD_EXIT command to ensure we are not in extended mode
    extChat(proto::SimplePacket<proto::CMD_EXIT>(), proto::SimplePacket<proto::CMD_EXIT>());

    QString response = txrx("#1ZP\r");
    QRegExp rx = QRegExp(".*1ZP\\+\\d$");
    if (rx.exactMatch(response))
    {
        qDebug() << "Found robot";
        robotIsConnected = true;
        emit robotConnectionChanged(true);
        emit robotConnected();
        emit message("ROBOT connected. Please initialize.");
    }
    else
    {
        if(!response.isEmpty())
            qDebug() << response;
        setPortNumber((portNumber+1) % PORTCYCLE);
    }
}

/**
 * Make sure the robot is in P0 state (non-initialized) before starting the initialization process.
 */
void RobotInterface::handle_robotReset()
{
    // If the software was closed, but the robot was not turned off,
    // then it's still in P2 (initialized). We have to manually reset.
    // And make sure that the robot received the commands.

    bool isReset = true;

    QHash<QString, MotorData>::iterator it;
    for(it = m_motors.begin(); it != m_motors.end(); ++it)
    {
        MotorData* m = &it.value();
        int a = m->joint.address;

        if(m->isReset)
            continue;

        if(txrx(QString("#%1P0\r").arg(a)
                ).endsWith(QString("%1P0").arg(a)))
        {
            m->isReset = true;
        }
        else
            isReset = false;
    }

    robotIsReset = isReset;
}

/**
 * Don't execute, only check if the robot has a valid initialization.
 * This is the case when all motors are in state P2. This check is
 * useful after start up or in case the robot was disconnected for a
 * while, so that an already initialized robot doesn't have to be
 * initialized again.
 */
void RobotInterface::handle_checkInitialization()
{
    bool isInitialized = true;

    QHash<QString, MotorData>::iterator it;
    for(it = m_motors.begin(); it != m_motors.end(); ++it)
    {
        MotorData* m = &it.value();

        if(m->isInitialized)
            continue;

        QString response = txrx(QString("#%1ZP\r").arg(m->joint.address));
        if(response.endsWith(QString("%1ZP+2").arg(m->joint.address)))
            m->isInitialized = true;
        else
            isInitialized = false;
    }

    if (isInitialized)
    {
        robotIsInitialized = true;
        doInitialize = false;

        // Collect a first set of encoder and motor feedback so that the tensions are initialized with 0.
        QHash<QString, MotorData>::iterator it;
        for(it = m_motors.begin(); it != m_motors.end(); ++it)
        {
            const MotorData& m = it.value();

            if(pollRegister(QString("%1I").arg(m.joint.address)))
                rxJointAngles[it.key()] = (registerValue * m.joint.enc_to_rad) - m.joint.offset;
        }

        lastRxJointAngles = rxJointAngles;

        QueryPerformanceCounter(&lastTime);
        emit robotInitialized();
        emit message("ROBOT is already initialized.");
    }

    doCheckInitialization = false;
}

/**
 * Check with a status request if the servos are initialized
 * and send out an initialization request where needed.
 */
void RobotInterface::handle_initialize()
{
    bool isInitialized = true;

    QHash<QString, MotorData>::iterator it;
    for(it = m_motors.begin(); it != m_motors.end(); ++it)
    {
        MotorData* m = &it.value();

        if(m->isInitialized)
            continue;

        QString response = txrx(QString("#%1ZP\r").arg(m->joint.address));
        if(response.endsWith(QString("%1ZP+2").arg(m->joint.address)))
            m->isInitialized = true;
        else
        {
            qDebug() << response;
            isInitialized = false;
            if(response.endsWith(QString("%1ZP+0").arg(m->joint.address)))
                txrx(QString("#%1P1\r").arg(m->joint.address));
        }
    }

    if (isInitialized)
    {
        robotIsInitialized = true;
        doInitialize = false;

        // Collect a first set of encoder and motor feedback so that the tensions are initialized with 0.
        QHash<QString, MotorData>::iterator it;
        for(it = m_motors.begin(); it != m_motors.end(); ++it)
        {
            const MotorData& m = it.value();

            if(pollRegister(QString("%1I").arg(m.joint.address)))
                rxJointAngles[it.key()] = (registerValue * m.joint.enc_to_rad) - m.joint.offset;
        }

        lastRxJointAngles = rxJointAngles;

        QueryPerformanceCounter(&lastTime);
        emit robotInitialized();
        emit message("Initialization complete. ROBOT is ready for your command.");

        if(extEnable() && extSendConfig(0))
        {
            m_isExtendedMode = true;
            m_isPlaying = false;
        }
    }
}

void RobotInterface::handle_extendedMode()
{
    // Measure how much real time passed since the last iteration.
    QueryPerformanceCounter(&tick);
    double timePassed = ((double)tick.QuadPart - (double)lastTime.QuadPart) / (double)ticksPerSecond.QuadPart;
    lastTime = tick;

    proto::Packet<proto::CMD_FEEDBACK, proto::Feedback> feedback;

    if(m_isPlaying || complianceMode == hardwareCompliance)
    {
        // Request feedback without giving a motion command
        if(!extCommand(proto::SimplePacket<proto::CMD_FEEDBACK>(), &feedback))
        {
            qDebug() << "no playback feedback";
            m_isExtendedMode = false;
            return;
        }
        else
            m_noFeedbackCounter = 0;

        if(m_isPlaying && m_stopPlaying)
        {
            // Send stop packets until FF_PLAYING flag in feedback vanishes
            qDebug() << "Sending stop command:" <<
            extChat(proto::SimplePacket<proto::CMD_STOP>(), proto::SimplePacket<proto::CMD_STOP>());
        }
    }
    else
    {
        // Request feedback with a motion command
        proto::Packet<proto::CMD_MOTION, proto::Motion> motion;

        // Limit the joint target angles to protect the joint limits.
        foreach (QString key, rxJointAngles.keys())
        {
            const MotorData& m = m_motors[key];
            txJointAngles[key] = qBound(
                m.joint.lower_limit,
                txJointAngles[key],
                m.joint.upper_limit
            );
        }

        motion.payload.num_axes = 0;
        QHash<QString, MotorData>::iterator it;
        for(it = m_motors.begin(); it != m_motors.end(); ++it)
        {
            const MotorData& m = it.value();
            int a = m.joint.address;
            double sgn = m.joint.invert ? -1 : 1;
            motion.payload.ticks[a-1] = qRound((sgn * txJointAngles[it.key()] + m.joint.offset) / m.joint.enc_to_rad) + proto::NT_POSITION_BIAS;
            motion.payload.velocity[a-1] = qMax(1, qAbs(qRound((txJointVelocities[it.key()]) / m.joint.mot_to_rad)));
            if(a > motion.payload.num_axes)
                motion.payload.num_axes = a;
        }

        motion.payload.output_command = txOutputCommand;
        motion.updateChecksum();

        if(!extCommand(motion, &feedback))
        {
            //disconnectRobot();
            m_isExtendedMode = false;
            return;
        }
    }

    foreach(const MotorData& m, m_motors.values())
    {
        const QString& key = m.joint.name;
        double sgn = m.joint.invert ? -1 : 1;
        int ticks = feedback.payload.positions[m.joint.address-1];

        if(ticks == 0x7FFF)
            continue;

        rxJointAngles[key] = sgn * (ticks * m.joint.enc_to_rad - m.joint.offset);

        rxJointVelocities[key] = qAbs(rxJointAngles[key] - lastRxJointAngles[key]) / timePassed;
    }

    lastRxJointAngles = rxJointAngles;

    if(complianceMode == hardwareCompliance)
        txJointAngles = rxJointAngles;

    if(m_isPlaying && !(feedback.payload.flags & proto::FF_PLAYING))
    {
        message("Playback finished.");
        m_isPlaying = false;

        // Halt if no other command is present
        txJointAngles = rxJointAngles;
        foreach(const MotorData& m, m_motors.values())
        {
            // motors give strange sounds if velocity == 0
            txJointVelocities[m.joint.name] = 1.0 * M_PI / 180.0 / m.joint.mot_to_rad;
        }

        emit playbackFinished();
        return;
    }

    // Broadcast the received joint angles and velocities to any receivers.
    emit motionOut(rxJointAngles, rxJointVelocities);
}

void RobotInterface::handle_checkComplianceMode()
{
    if(complianceMode == requestedComplianceMode)
        return;

    if(requestedComplianceMode == noCompliance)
    {
        // Send out an initial command packet before switching
        txJointAngles = rxJointAngles;
        ComplianceMode lastComp = complianceMode;
        complianceMode = noCompliance;
        handle_extendedMode();
        complianceMode = lastComp;
    }

    // We need to exit extended mode in order to talk to the motor controllers.
    if(!extDisable())
        return;

    // Timeout on compliance change
    QTimer timer;
    timer.setInterval(2000);
    timer.start();

    if(requestedComplianceMode == hardwareCompliance)
    {
        while(1)
        {
            bool isHardwareCompliant = true;

            if(!timer.isActive())
            {
                message("<font color=\"red\">Failed to change to hardware compliance mode.</font>");
                requestedComplianceMode = complianceMode;
                break;
            }

            QHash<QString, MotorData>::iterator it;
            for(it = m_motors.begin(); it != m_motors.end(); ++it)
            {
                MotorData* m = &it.value();

                if(m->isHWCompliant)
                    continue;

                int a = m->joint.address;

                if(txrx(QString("#%1r0\r").arg(a)).endsWith(QString("%1r0").arg(a))
                        && txrx(QString("#%1i0\r").arg(a)).endsWith(QString("%1i0").arg(a)))
                {
                    m->isHWCompliant = true;
                }
                else
                    isHardwareCompliant = false;
            }

            if(isHardwareCompliant)
            {
                emit message("<font color=\"green\">The robot is in hardware compliance mode.</font>");

                break;
            }
        }
    }

    else if(requestedComplianceMode == noCompliance)
    {
        while(1)
        {
            bool stiff = true;

            if(!timer.isActive())
            {
                message("<font color=\"red\">Failed to change to hardware compliance mode.</font>");
                requestedComplianceMode = complianceMode;
                break;
            }

            QHash<QString, MotorData>::iterator it;
            for(it = m_motors.begin(); it != m_motors.end(); ++it)
            {
                MotorData* m = &it.value();

                if(!m->isHWCompliant)
                    continue;

                int a = m->joint.address;

                if(txrx(QString("#%1r%2\r").arg(a).arg(m->joint.hold_current)).endsWith(QString("%1r%2").arg(a).arg(m->joint.hold_current))
                        && txrx(QString("#%1i%2\r").arg(a).arg(m->joint.max_current)).endsWith(QString("%1i%2").arg(a).arg(m->joint.max_current)))
                {
                    m->isHWCompliant = false;
                }
                else
                    stiff = false;
            }

            if (stiff)
            {
                emit message("<font color=\"green\">The Robot is stiff.</font>");
                break;
            }
        }
    }

    // Enable extended mode again
    extEnable();

    complianceMode = requestedComplianceMode;
    emit complianceChanged(complianceMode);
}


/*
 * The step method is the main iteration of the robot interface. With every iteration the interface tries to
 * exchange packets with the robot. Depending on the connection state (port connected or not, robot detected
 * or not), the iteration loop tries to recover the connection, to detect the robot or it pursues the normal
 * packet exchange.
 */
void RobotInterface::step()
{
    // Setup the port if not done yet.
    if (!serial.isOpen())
    {
        QString portName = "\\\\.\\COM" + QString::number(portNumber);
        if (serial.Open(portName))
        {
            qDebug() << "trying" << portName;
            serial.Setup(CSerial::EBaud115200, CSerial::EData8, CSerial::EParNone, CSerial::EStop1);
            serial.SetupHandshaking(CSerial::EHandshakeOff);
            serial.SetMask(CSerial::EEventRecv);
            serial.SetEventChar(0x0D);
        }
        else
        {
            setPortNumber((portNumber+1) % PORTCYCLE);
        }
    }

    // Confirm the connection with a status query and load the calibration.
    else if (!robotIsConnected)
    {
        handle_confirmConnection();
    }

    else if (!robotIsReset)
        handle_robotReset();
    else if (doCheckInitialization)
        handle_checkInitialization();
    else if (doInitialize)
        handle_initialize();

    // Switch the robot to extended Mode
    else if(!m_isExtendedMode)
    {
        if(extEnable())
            m_isExtendedMode = true;
    }

    // Handle extended mode communication (i.e. communication with µC)
    else if(m_isExtendedMode)
    {
        handle_checkComplianceMode();
        handle_extendedMode();
    }
}

void RobotInterface::run()
{
    // Run Qt event loop
    exec();

    // Leave robot in a nice state
    extDisable();
}

//@}
// END COMMUNICATION THREAD CODE
