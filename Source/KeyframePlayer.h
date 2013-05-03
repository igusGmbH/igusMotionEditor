/*
 * KeyframePlayer.h
 *
 * This class plays the current keyframes in the motion sequence.
 *
 *  Created on: Feb 9, 2009
 *  Author: Marcell Missura, missura@ais.uni-bonn.de
 */

#ifndef KEYFRAMEPLAYER_H_
#define KEYFRAMEPLAYER_H_
#include <QObject>
#include <QList>
#include <QHash>
#include <QString>
#include <QPointer>
#include <QTimer>

#include "microcontroller/protocol.h"

#include "KeyframePlayerItem.h"

class KeyframePlayer : public QObject
{
	Q_OBJECT

	KeyframePlayerItem* head;
	KeyframePlayerItem* current;
	double sliderPosition;
	double speedLimit;
	double timeCorrection;
	double velocityAdaptionStrength;

	QTimer timer;

	// Precise timer.
	LARGE_INTEGER startTime;
	LARGE_INTEGER lastTime;
	LARGE_INTEGER ticksPerSecond;
	LARGE_INTEGER tick;

public:

	QHash<QString, double> rxJointAngles;
	QHash<QString, double> txJointAngles;
	QHash<QString, double> txJointVelocities;
	QHash<QString, double> txJointVelocityCorrectionFactors;

	bool looped;
	bool interpolating;
    bool velocityAdaption;

	KeyframePlayer();
	virtual ~KeyframePlayer();
	void playTheseFrames(QList< QPointer<Keyframe> >);
    void start();
	void stop();
	bool isPlaying();

    const KeyframePlayerItem* playingList();

public slots:
	void setSpeedLimit(int sl);
	void jointAnglesIn(QHash<QString, double>);
	void setTimeCorrection(int cor);

signals:
	void motionOut(QHash<QString, double>, QHash<QString, double>);
	void finished();

private slots:
	void step();
};

#endif /* KEYFRAMEPLAYER_H_ */
