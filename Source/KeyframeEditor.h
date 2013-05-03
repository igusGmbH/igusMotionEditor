#ifndef KEYFRAMEEDITOR_H
#define KEYFRAMEEDITOR_H

#include "ui_KeyframeEditor.h"
#include <QtGui>

#include "Keyframe.h"
#include "RobotView3D.h"

class KeyframeEditor : public QGroupBox
{
    Q_OBJECT

    struct GUIElements
    {
        QSlider* slider;
        QDoubleSpinBox* spinBox;
    };
    QHash<QString, GUIElements> m_guiElements;

    Ui::KeyframeEditorClass ui;

    static const double degToRad;
    static const double radToDeg;

	QTime lastTime; // It's a protection against performance problems.

	RobotView3D *robotView;

	double speedLimit;
	QHash<QString, double> txJointAngles;
	QHash<QString, double> txJointVelocities;

    JointInfo::ListPtr m_jointConfig;
    QPointer<Keyframe> m_keyframe;
public:
    KeyframeEditor(QWidget *parent = 0);
    int getSpeed();
    double getPause();
    QHash<QString, double> getJointAngles();
    int getOutputCommand();

    inline bool isLoaded() const
    { return m_keyframe != 0; }

    inline Keyframe* loadedKeyframe() const
    { return m_keyframe; }

public slots:
	void motionIn(QHash<QString, double>, QHash<QString, double>);
	void joystickIn(QHash<QString, double>);
	void setJointAngles(QHash<QString, double>);
	void setSpeedLimit(int sl);
	void setSpeed(int);
	void setPause(double);
    void setOutputCommand(int);
	void zeroKeyframe();
    void setJointConfig(const JointInfo::ListPtr& config);
    void loadKeyframe(Keyframe* keyframe);
    void unloadKeyframe();

private slots:
	void jointAnglesChangedBySpinbox();
	void jointAnglesChangedBySlider();
	void jointAnglesChangedByInternalView();
    void processOutputCommandChange(int oc);

signals:
	void spinboxValueChanged();
	void sliderValueChanged();
	void keyframeDropped(Keyframe*);
	void speedChanged(int);
	void pauseChanged(double);
    void outputCommandChanged(int);
    void motionOut(QHash<QString, double>, QHash<QString, double>, int outputCommand);

    void saveRequested();

protected:
	void dragEnterEvent(QDragEnterEvent*);
	void dropEvent(QDropEvent*);

private:
	void transferJointAnglesToGuiElements();

};

#endif // KEYFRAMEEDITOR_H
