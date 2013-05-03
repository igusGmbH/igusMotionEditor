/*
 * Keyframe.h
 *
 *  Created on: Jan 14, 2009
 *  Author: Marcell Missura, missura@ais.uni-bonn.de
 */
#ifndef KEYFRAME_H
#define KEYFRAME_H

#include <QtGui>
#include <QString>
#include <QHash>
#include <QLabel>

#include "RobotView3D.h"

extern const char* DIGITAL_OUTPUT_LABELS[];

class Keyframe : public QWidget
{
	Q_OBJECT

	// GUI elements
	RobotView3D* robotView;
	QLabel* indexLabel;
	QSpinBox* speedBox;
	QDoubleSpinBox* pauseBox;
    QComboBox* digBox;
    QLabel* robotViewContainer;

	// Indicates the position of the keyframe in a motion sequence.
	int index;

	// Amount of seconds to stay in this keyframe before continuing with the next one.
	double pause;

	// The speed parameter is a percental value (1 - 100) that describes how fast this keyframe
	// should be reached. When the speed is 100, the robot will try to reach the keyframe as fast
	// as possible. A speed of 1 is really slow. Setting 0 as speed should be avoided, because
	// the robot will not move at all and the program will freeze while playing the keyframes.
	int speed;

	bool selected;
	bool loaded;

	bool ignoreMouse;

public:
	QHash<QString, double> jointAngles;
	QPixmap modelPixmap;

    //! See labels in Keyframe.cpp
    enum DigitalOutput
    {
        DO_IGNORE, //!< Do nothing
        DO_SET,    //!< Set output
        DO_RESET,  //!< Reset output

        DO_COUNT
    };

	Keyframe(QWidget*);
	virtual ~Keyframe();

	int getIndex();
	int getSpeed();
    DigitalOutput getOutputCommand() const;
	double getPause();
	void setIndex(int);
	void toggleSelected();
	void setSelected(bool);
	bool isSelected();
	void setLoaded(bool);
	bool isLoaded();
	const QString toString();
	void fromString(const QString);
	void zoomIn();
	void zoomOut();
	void setZoom(int zoomFactor);
	double distance(Keyframe*);
	double distance(QHash<QString, double>);

	static bool validateString(QString);
	static QHash<QString, double> jointAnglesFromString(QString);

public slots:
	void motionIn(QHash<QString, double>);
	void setJointAngles(QHash<QString, double>);
	void setPause(double);
	void setSpeed(int);
    void setOutputCommand(int cmd);
	void updatePixmap();
    void setJointConfig(const JointInfo::ListPtr& config);

    void updateView();

signals:
	void jointAnglesChanged(QHash<QString, double>);
	void speedChanged(int);
	void pauseChanged(double);

    //! cmd is a DigitalOutput member
    void outputCommandChanged(int cmd);

private slots:
	void jointAnglesChangedByInternalView();
	void speedChangedBySpinbox();
	void pauseChangedBySpinbox();

protected:
	void paintEvent(QPaintEvent*);
	void mouseMoveEvent(QMouseEvent* e);
	void mousePressEvent(QMouseEvent* e);
	void mouseReleaseEvent(QMouseEvent* e);
	void mouseDoubleClickEvent(QMouseEvent* e);
	void keyPressEvent(QKeyEvent*);
};

#endif /* KEYFRAME_H */
