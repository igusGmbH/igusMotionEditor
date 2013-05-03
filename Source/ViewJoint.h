// 3D view representation of one joint
// Author: Max Schwarz <max.schwarz@uni-bonn.de>

#ifndef VIEWJOINT_H
#define VIEWJOINT_H

#include <QGLViewer/manipulatedFrame.h>

#include "JointConfiguration.h"

using namespace qglviewer;

class ViewJoint
{
public:
    ViewJoint(const JointInfo& info);

    virtual void draw(GLUquadric* quadric, int slices, bool selected) = 0;
    virtual void setReferenceFrame(Frame* frame);
    virtual void setJointAngle(double angle) = 0;
    virtual double jointAngle() const = 0;
    virtual Vec connectionPoint() const = 0;

    inline const JointInfo& info()
    { return m_jointInfo; }

    inline ManipulatedFrame* frame()
    { return &m_frame; }

    static ViewJoint* factory(const JointInfo& info);
protected:
    ManipulatedFrame m_frame;
    JointInfo m_jointInfo;
    double m_length;
};

class ViewJointX : public ViewJoint
{
public:
    ViewJointX(const JointInfo& info);

    virtual void draw(GLUquadric* quadric, int slices, bool selected);
    virtual void setJointAngle(double angle);
    virtual double jointAngle() const;
    virtual Vec connectionPoint() const;

    double length() const;
};

class ViewJointZ : public ViewJoint
{
public:
    ViewJointZ(const JointInfo& info);

    virtual void draw(GLUquadric* quadric, int slices, bool selected);
    virtual void setJointAngle(double angle);
    virtual double jointAngle() const;
    virtual Vec connectionPoint() const;

    double length() const;
};

#endif // VIEWJOINT_H
