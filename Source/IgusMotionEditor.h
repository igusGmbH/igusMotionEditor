
#ifndef MOTIONEDITOR_H
#define MOTIONEDITOR_H

#include <QtGui>
#include <QString>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileSystemModel>
#include <QPointer>
#include <QTimer>
#include <QTime>
#include <QKeyEvent>

#include "ui_igusmotioneditor.h"
#include "Keyframe.h"
#include "KeyframePlayer.h"
#include "KeyframeEditor.h"
#include "KeyframeArea.h"
#include "RobotInterface.h"
#include "JoystickControl.h"
#include "JointConfiguration.h"

class IgusMotionEditor : public QWidget
{
    Q_OBJECT

    Ui::IgusMotionEditorClass ui;

    QFileSystemModel *fileSystemModel;

    enum RobotState
	{
        Off,
		Stiff,
		SoftwareCompliant,
        HardwareCompliant
	};

	RobotState robotState;

	KeyframeEditor* keyframeEditor;
	KeyframeArea* motionSequence;
	KeyframeArea* sandbox;

	RobotInterface robotInterface;
	KeyframePlayer keyframePlayer;
    JoystickControl joystickControl;
    JointConfiguration jointConfiguration;

    QPixmap robolinkIconOrange;
    QPixmap robolinkIconGrey;
    QPixmap joystickIconOrange;
    QPixmap joystickIconGrey;

    QTime lastFrameGrabbedTime;
    int framesToGrab;
    int framesPerSecond;
    bool isGrabbing;
    bool m_isPlaying;

    QProgressBar m_flashProgressBar;

public:
	IgusMotionEditor(QWidget *parent = 0);
	~IgusMotionEditor(){};

signals:
    void complianceChangeRequested(int mode);
    void keyframeTransferRequested(const KeyframePlayerItem* head, int cmd);

protected:
	void keyPressEvent(QKeyEvent* event);

private slots:
	void loadKeyframe(Keyframe*);
    void loadUnloadKeyframe(Keyframe*);
    void saveKeyframe();

    void message(QString);

	void robotDisconnected();
	void robotConnected();
	void robotInitialized();
	void joystickConnected();
	void joystickDisconnected();

    void on_initButton_clicked();

	void on_startGrabButton_clicked();
	void grabFrame(QHash<QString, double>, QHash<QString, double>);
	void frameGrabberFinished();

	void on_playButton_clicked();
	void on_loopButton_clicked();
	void playerFinished();

    void on_flashButton_clicked();

	void on_saveButton_clicked();
	void on_loadButton_clicked();
	void on_deleteFileButton_clicked();

	void on_interpolateSandbox_clicked();
	void on_interpolateMotionSequence_clicked();

	void on_motionSpeedSlider_valueChanged(int);
	void on_alignSpeedSlider_valueChanged(int);

    void setRobotState(int);
    void complianceChanged(int);

    void keyframeTransferFinished(bool success);
	void handleConnections();
};

#endif // MOTIONEDITOR_H
