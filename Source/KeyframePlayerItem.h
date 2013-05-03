/*
 * KeyframePlayerItem.h
 *
 *  Created on: 13.03.2009
 *      Author: missura
 */
#ifndef KEYFRAMEPLAYERITEM_H_
#define KEYFRAMEPLAYERITEM_H_

#include "Keyframe.h"

class KeyframePlayerItem
{
public:
	KeyframePlayerItem();
	virtual ~KeyframePlayerItem();

    struct AxisInfo
    {
        AxisInfo()
        {
        }

        AxisInfo(double _angle, double _velocity)
         : angle(_angle), velocity(_velocity)
        {
        }

        double angle;
        double velocity;
    };

public:
    QHash<QString, AxisInfo> joints;
	double relativeTime;
	double absoluteTime;
    int outputCommand;
	KeyframePlayerItem* next;

    void setJointAngles(const QHash<QString, double>& jointAngles);
    QHash<QString, double> jointAngles();
};

#endif /* KEYFRAMEPLAYERITEM_H_ */
