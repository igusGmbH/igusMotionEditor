/* The keyframe is the smallest unit of the data structure that is built up in the keyframe
 * player. One keyframe defines a joint angle and a position in time for a single joint. It
 * also contains a pointer to the next keyframe that follows in the time line.
 *
 * Author: missura
 */

#include "KeyframePlayerItem.h"

KeyframePlayerItem::KeyframePlayerItem()
{
	relativeTime = 0;
	absoluteTime = 0;
    next = 0;
    outputCommand = Keyframe::DO_IGNORE;
}

/*
 * Destroys the keyframe.
 * ATTENTION! The keyframe also destroys the next keyframe it's pointing at.
 */
KeyframePlayerItem::~KeyframePlayerItem()
{
	if (next)
		delete next;
}

void KeyframePlayerItem::setJointAngles(const QHash<QString, double> &jointAngles)
{
    QHash<QString, double>::const_iterator it;
    for(it = jointAngles.begin(); it != jointAngles.end(); ++it)
    {
        joints[it.key()].angle = it.value();
    }
}

QHash<QString, double> KeyframePlayerItem::jointAngles()
{
    QHash<QString, double> ret;
    QHash<QString, AxisInfo>::iterator it;
    for(it = joints.begin(); it != joints.end(); ++it)
        ret[it.key()] = it.value().angle;

    return ret;
}


