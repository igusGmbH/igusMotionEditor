/*
 * RobotView3D.cpp
 *
 * This class encapsulates an OpenGL 3D view of a robot model.
 * The pose of the robot is determined by a pointer to a set of
 * joint angles. The 3D view is always embedded into either a
 * keyframe or the keyframe editor on the top left, so the
 * pointer to the joint angles should always point to the joint
 * angles stored inside the embedding object.
 *
 *  Created on: Jan 21, 2009
 *  Author: Marcell Missura, missura@ais.uni-bonn.de
 */
#include <QtGui>
#include <math.h>
#include <QGLViewer/qglviewer.h>
#include "globals.h"
#include "RobotView3D.h"
#include <QDebug>

using namespace qglviewer;
using namespace std;

RobotView3D::RobotView3D(QWidget *parent) :
	QGLViewer(parent)
{
	jointAngles = NULL; // Pointer to the joint angles data structure.
	selected = -1; // Which segment is selected? -1 means nothing is selected.
	slices = 64; // Influences the graphical detail.
	ignoreMouse = false;

	// This will stop saving the annoying little xml files.
    setStateFileName(QString::null);
}

RobotView3D::~RobotView3D()
{
}

void RobotView3D::setJointConfig(const JointInfo::ListPtr &config)
{
    m_jointConfig = config;

    foreach(ViewJoint* joint, m_viewJoints)
        delete joint;
    m_viewJoints.clear();

    if(!config)
    {
        update();
        return;
    }

    Frame* currentBaseFrame = &baseFrame;
    Vec connectionPoint(0, 0, 0.26);
    double armLength = 0;
    foreach(const JointInfo& info, *config)
    {
        ViewJoint* joint = ViewJoint::factory(info);
        if(!joint)
            exit(2);

        joint->setReferenceFrame(currentBaseFrame);
        joint->frame()->setTranslation(connectionPoint);
        joint->frame()->setSpinningSensitivity(100.0);

        currentBaseFrame = joint->frame();
        connectionPoint = joint->connectionPoint();
        armLength += connectionPoint.norm();

        m_viewJoints << joint;
    }

    setSceneCenter(Vec(0, 0, (0.26+armLength)/2));
    setSceneRadius(armLength);
    camera()->lookAt(sceneCenter());
    showEntireScene();

    update();
}

void RobotView3D::init()
{
	setBackgroundColor(QColor(255, 255, 255, 255));

	// Setup the camera position.
    camera()->setPosition(Vec(1.3, 0, 0.5));
	camera()->setUpVector(Vec(0.0, 0.0, 1.0));
	camera()->lookAt(Vec(0.0, 0.0, 0.4));
    camera()->setRevolveAroundPoint(Vec(0, 0, 0));
    camera()->showEntireScene();


	setMouseBinding(Qt::MidButton, SELECT);

	// Make camera the default manipulated frame.
	setManipulatedFrame(camera()->frame());

    // Disable spinning for camera
    camera()->frame()->setSpinningSensitivity(100.0);

	// Light setup
	glEnable(GL_LIGHT1);

	// Light default parameters
	const GLfloat light_ambient[4]  = {0.2, 0.2, 0.2, 0.2};
	const GLfloat light_specular[4] = {0.2, 0.2, 0.2, 0.2};
	const GLfloat light_diffuse[4]  = {0.2, 0.2, 0.2, 0.2};
	glLightfv(GL_LIGHT1, GL_AMBIENT,  light_ambient);
	glLightfv(GL_LIGHT1, GL_SPECULAR, light_specular);
	glLightfv(GL_LIGHT1, GL_DIFFUSE,  light_diffuse);

	// Shining.
	glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 60.0);
	GLfloat specular_color[4] = { 0.8f, 0.8f, 0.8f, 0.5 };
	glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, specular_color);

	setHandlerKeyboardModifiers(QGLViewer::CAMERA, Qt::AltModifier);
    setHandlerKeyboardModifiers(QGLViewer::FRAME, Qt::NoModifier);
    setHandlerKeyboardModifiers(QGLViewer::CAMERA, Qt::ControlModifier);

	glEnable(GL_POINT_SMOOTH);
	glEnable(GL_LINE_SMOOTH);
}

/*
 * Updates the view by applying the currently set joint angles to the kinematic model.
 */
void RobotView3D::updateView()
{
	if (!jointAngles)
		return;

    foreach(ViewJoint* joint, m_viewJoints)
    {
        if(!jointAngles->contains(joint->info().name))
            continue;

        joint->setJointAngle(jointAngles->value(joint->info().name));
    }

	update();
}

/*
 * Called after a shift-click for selecting one of the robot's segments.
 */
void RobotView3D::postSelection(const QPoint&)
{
    selected = selectedName();

    setHandlerKeyboardModifiers(QGLViewer::CAMERA, Qt::AltModifier);
    setHandlerKeyboardModifiers(QGLViewer::FRAME, Qt::NoModifier);
    setHandlerKeyboardModifiers(QGLViewer::CAMERA, Qt::ControlModifier);

    if(selected > 0 && selected <= m_viewJoints.size())
        setManipulatedFrame(m_viewJoints[selected-1]->frame());
    else
        setManipulatedFrame(camera()->frame());
}


/*
 * This function generally handles mouse move events, but it's also used for the specific
 * purpose of detecting when the limbs of the robot are moved manually, so that the
 * embedding keyframe or keyframe editor can be updated in real time.
 */
void RobotView3D::mouseMoveEvent(QMouseEvent* e)
{
	if (ignoreMouse)
	{
		e->ignore();
		return;
	}

    // First do what QGLViewer would do.
    QGLViewer::mouseMoveEvent(e);

	// If something is selected and the jointAngles are pointing to something valid.
	if (selected != -1 && jointAngles)
	{
		// Derive and change the joint angles from the frame positions.

        foreach(ViewJoint* joint, m_viewJoints)
        {
            if(!jointAngles->contains(joint->info().name))
                continue;

            (*jointAngles)[joint->info().name] = joint->jointAngle();
        }

		// Emit a signal.
		emit jointAnglesChanged();
	}
}

void RobotView3D::mousePressEvent(QMouseEvent* e)
{
	if (ignoreMouse)
	{
		e->ignore();
		return;
	}

	QGLViewer::mousePressEvent(e);
}

void RobotView3D::mouseReleaseEvent(QMouseEvent* e)
{
	if (ignoreMouse)
	{
		e->ignore();
		return;
	}

    QGLViewer::mouseReleaseEvent(e);
}

/*
 * Double click resets the camera to an initial view.
 */
void RobotView3D::mouseDoubleClickEvent(QMouseEvent* e)
{
	if (ignoreMouse)
	{
		e->ignore();
		return;
	}

	// Setup the camera position.
	camera()->setPosition(Vec(1.3, 0.2, 0.5));
	camera()->setUpVector(Vec(0.0, 0.0, 1.0));
	camera()->lookAt(Vec(0.0, 0.0, 0.4));
	//camera()->showEntireScene();
	update();
}

// Renders a pixmap from the 3D view according to the currently set joint angles.
QPixmap RobotView3D::getPixmap(int width, int height)
{
	if (width > 0 && height > 0)
	{
		QRect currentSize = geometry();
		setGeometry(0,0,width, height);
		QPixmap px = renderPixmap(width, height,true);
		//qDebug() << px.width() << px.height() << ((double)px.width()/px.height()) << currentSize.width() << currentSize.height() << ((double)currentSize.width()/currentSize.height());
		setGeometry(currentSize);
		return px;
	}
	else
		return renderPixmap(0,0,true);
}

/*
 * Draws the kinematic model with "names" pushed on the OpenGL stack to support selection with the mouse.
 */
void RobotView3D::drawWithNames()
{
	static GLUquadric* quadric = gluNewQuadric();

	// Draw the base.
	glPushName(0);
    glColor3f(0.4f, 0.41f, 0.48f);
    gluDisk(quadric, 0.0, 0.1, slices, 1);
    gluCylinder(quadric, 0.03, 0.03, 0.29, slices, 1);
	gluCylinder(quadric, 0.1, 0.1, 0.03, slices, 1);
	glTranslatef(0.0, 0.0, 0.03);
	gluCylinder(quadric, 0.1, 0.03, 0.03, slices, 1);
	glPopName();

    // Draw joints
    glColor3f(0.5f, 0.51f, 0.58f);
    for(int i = 0; i < m_viewJoints.size(); ++i)
    {
        glPushName(i+1);
        m_viewJoints[i]->draw(quadric, slices, false);
        glPopName();
    }
}

/*
 * Draws the kinematic model in the OpenGL environment.
 */
void RobotView3D::draw()
{
//	qDebug() << "draw";

	static GLUquadric* quadric = gluNewQuadric();

	// Draw the base.
    glColor3f(0.4f, 0.41f, 0.48f);
    gluDisk(quadric, 0.0, 0.1, slices, 1);
    gluCylinder(quadric, 0.03, 0.03, 0.29, slices, 1);
	gluCylinder(quadric, 0.1, 0.1, 0.03, slices, 1);
	glTranslatef(0.0, 0.0, 0.03);
	gluCylinder(quadric, 0.1, 0.03, 0.03, slices, 1);

    // Draw joints
    glColor3f(0.5f, 0.51f, 0.58f);
    for(int i = 0; i < m_viewJoints.size(); ++i)
        m_viewJoints[i]->draw(quadric, slices, selected == i+1);
}

