/*
 * RobotView3D.h
 *
 *  Created on: Jan 21, 2009
 *  Author: Marcell Missura, missura@ais.uni-bonn.de
 */

#ifndef ROBOTVIEW3D_H_
#define ROBOTVIEW3D_H_

#include <QtGui>
#include <QHash>
#include <QString>
#include <QGLViewer/qglviewer.h>

#include "ViewJoint.h"

using namespace qglviewer;

class RobotView3D: public QGLViewer
{
	Q_OBJECT

	QHash<QString, double> *jointAngles;
	unsigned short selected;
	int slices;

	Frame baseFrame;
    QList<ViewJoint*> m_viewJoints;

    JointInfo::ListPtr m_jointConfig;
public:
	RobotView3D(QWidget*);
	virtual ~RobotView3D();

	bool ignoreMouse;

	void setJointAngles(QHash<QString, double> *ja) { jointAngles = ja; }
	QHash<QString, double>* getJointAngles() { return jointAngles; }
	QPixmap getPixmap(int width=0, int height=0);

public slots:
	void updateView();
    void setJointConfig(const JointInfo::ListPtr& config);

signals:
	void jointAnglesChanged();

protected:
	virtual void draw();
	virtual void init();
	virtual void drawWithNames();
	virtual void postSelection(const QPoint& point);
	void mouseMoveEvent(QMouseEvent* e);
	void mousePressEvent(QMouseEvent* e);
	void mouseReleaseEvent(QMouseEvent* e);
	void mouseDoubleClickEvent(QMouseEvent* e);
};

#endif /* ROBOTVIEW3D_H_ */
