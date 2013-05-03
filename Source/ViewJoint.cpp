// 3D view representation of one joint
// Author: Max Schwarz <max.schwarz@uni-bonn.de>

#include "ViewJoint.h"

#include <QDebug>
#include <QGLViewer/qglviewer.h>

#include "globals.h"

using namespace std;

ViewJoint::ViewJoint(const JointInfo& info)
 : m_jointInfo(info)
{
}

void ViewJoint::setReferenceFrame(Frame *frame)
{
    m_frame.setReferenceFrame(frame);
}

ViewJoint* ViewJoint::factory(const JointInfo& info)
{
    if(info.type == "X")
        return new ViewJointX(info);
    else if(info.type =="Z")
        return new ViewJointZ(info);

    qDebug() << "ViewJoint::factory(): unknown type" << info.type;
    return 0;
}

// IMPLEMENTATION for X joints

ViewJointX::ViewJointX(const JointInfo& info)
 : ViewJoint(info)
{
    LocalConstraint* xAxisOnly = new LocalConstraint();
    xAxisOnly->setTranslationConstraint(AxisPlaneConstraint::FORBIDDEN, Vec(0, 0, 0));
    xAxisOnly->setRotationConstraint(AxisPlaneConstraint::AXIS, Vec(1, 0, 0));

    m_frame.setConstraint(xAxisOnly);
    m_frame.setTranslation(Vec(0, 0, 0.29));
}

void ViewJointX::setJointAngle(double angle)
{
    m_frame.setRotation(0, 0, 0, 1); // reset to 0 position
    m_frame.rotate(Quaternion(Vec(1,0,0), angle));
}

double ViewJointX::jointAngle() const
{
    const Quaternion& rot = m_frame.rotation();
    return rot.angle() * sgn(rot.axis()[0]);
}

double ViewJointX::length() const
{
    return (m_jointInfo.length >= 0) ? m_jointInfo.length : 0.09;
}

void ViewJointX::draw(GLUquadric* quadric, int slices, bool selected)
{
    glMultMatrixd(m_frame.matrix());
    if(selected)
        glColor3f(0.6f, 0.6f, 0.0);
    else
        glColor3f(0, 0, 0);

    gluCylinder(quadric, 0.03, 0.03, length(), slices, 1);
    gluSphere(quadric, 0.05, slices, slices);
}

Vec ViewJointX::connectionPoint() const
{
    return Vec(0, 0, length());
}

// IMPLEMENTATION for Z joints

ViewJointZ::ViewJointZ(const JointInfo& info)
 : ViewJoint(info)
{
    LocalConstraint* zAxisOnly = new LocalConstraint();
    zAxisOnly->setTranslationConstraint(AxisPlaneConstraint::FORBIDDEN, Vec(0, 0, 0));
    zAxisOnly->setRotationConstraint(AxisPlaneConstraint::AXIS, Vec(0, 0, 1));

    m_frame.setConstraint(zAxisOnly);
}

void ViewJointZ::setJointAngle(double angle)
{
    m_frame.setRotation(0, 0, 0, 1); // reset to 0 position
    m_frame.rotate(Quaternion(Vec(0,0,1), angle));
}

double ViewJointZ::jointAngle() const
{
    const Quaternion& rot = m_frame.rotation();
    return rot.angle() * sgn(rot.axis()[2]);
}

double ViewJointZ::length() const
{
    return (m_jointInfo.length >= 0) ? m_jointInfo.length : 0.20;
}

void ViewJointZ::draw(GLUquadric *quadric, int slices, bool selected)
{
    glMultMatrixd(m_frame.matrix());
    if(selected)
        glColor3f(0.6f, 0.6f, 0.0f);
    else
        glColor3f(0.5f, 0.51f, 0.58f);

    gluCylinder(quadric, 0.03, 0.03, length(), slices, 1);

    // Draw side handle to show rotation
    glPushMatrix();
    glTranslatef(0.0, 0.0, 0.01);
    glRotatef(-90,1,0,0);
    gluCylinder(quadric, 0.01, 0.01, 0.1, slices, 1);
    glTranslatef(0.0, 0.0, 0.1);
    gluDisk(quadric, 0.0, 0.01, slices, 1);
    glPopMatrix();

    // Draw top
    glPushMatrix();
    glTranslatef(0.0, 0.0, length());
    gluDisk(quadric, 0.0, 0.03, slices, 1);
    glPopMatrix();
}

Vec ViewJointZ::connectionPoint() const
{
    return Vec(0, 0, length());
}


