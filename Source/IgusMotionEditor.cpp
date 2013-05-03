/*
 * IgusMotionEditor.cpp
 *
 * This is the main application object. It's the starting point where all other objects are
 * instantiated and the graphical user interface is constructed and launched. The handling
 * of the majority of buttons, mouse clicks, keyboard events as well as internal events
 * and messages such as the successful establishment of the robot connection, connection of
 * a joystick etc. are handled in this object.
 *
 * Author: Marcell Missura, missura@ais.uni-bonn.de
 */
#include <QString>
#include <QRegExp>
#include <QHash>
#include <QHashIterator>
#include <QPointer>
#include <QHBoxLayout>
#include <QFile>
#include <QTimer>
#include <QTime>
#include <QTextStream>
#include <QModelIndex>
#include <QFileDialog>
#include <QTextStream>

#include "globals.h"
#include "IgusMotionEditor.h"
#include "KeyframeArea.h"
#include "Keyframe.h"
#include "RobotInterface.h"

//TODO Sometimes after a drop nothing is happening and the mouse has to be moved first.
//TODO The size of the rendered pixmap is not always right.
//TODO Why is the motor off when hitting the joint limit in compliant mode?
//TODO Hell brakes lose when switching from off to compliance mode.
//TODO Holonomic keyframe interpolation would be nice.
//TODO How about a thicker border for the keyframe areas on focus?
//TODO More thread safety of the robot interface structures.
//TODO Highlight selected sliders and spinboxes.

IgusMotionEditor::IgusMotionEditor(QWidget *parent)
    : QWidget(parent)
{
    ui.setupUi(this);

	// This takes the blinking cursor away.
	setFocus();

	message("Connecting to robot...");

	// The joint angles are always kept in a QHash<QString, double> associative data structure.
	// To be used in the signature of a signal, QHash<QString, double> needs to be registered as a meta type.
	// Read more about this here: http://doc.trolltech.com/4.2/qt.html#ConnectionType-enum
	qRegisterMetaType< QHash<QString, double> >("QHash<QString, double>");

	robolinkIconGrey.load("images/robolinkicon_grey.png");
	robolinkIconOrange.load("images/robolinkicon_orange.png");
	joystickIconGrey.load("images/joystick_grey.png");
	joystickIconOrange.load("images/joystick_orange.png");

	ui.igusLogo->setPixmap(QPixmap("images/igus_logo.png"));
	ui.robolinkIcon->setPixmap(robolinkIconOrange);
	ui.connectionStatusIndicator->setAlignment(Qt::AlignHCenter);
	ui.connectionStatusIndicator->setPixmap(robolinkIconGrey);
	ui.joystickStatusIndicator->setAlignment(Qt::AlignHCenter);
	ui.joystickStatusIndicator->setPixmap(joystickIconGrey);

	on_motionSpeedSlider_valueChanged(0);
	on_alignSpeedSlider_valueChanged(0);

	// The motion sequence editor in the middle.
	motionSequence = new KeyframeArea(ui.motionEditorScrollArea);
	motionSequence->setZoom(3);
	ui.motionEditorScrollArea->setWidget(motionSequence);
	connect(ui.clearMotionSequence, SIGNAL(clicked()), motionSequence, SLOT(clear()));
	connect(ui.clearMotionSequence, SIGNAL(clicked()), ui.filenameEdit, SLOT(clear()));
	connect(ui.deleteMotionSequence, SIGNAL(clicked()), motionSequence, SLOT(deleteSelected()));
	connect(motionSequence, SIGNAL(keyframeDoubleClick(Keyframe*)), this, SLOT(loadUnloadKeyframe(Keyframe*)));
	connect(motionSequence, SIGNAL(droppedFileName(QString)), ui.filenameEdit, SLOT(setText(QString)));

	// The sandbox on the bottom.
	sandbox = new KeyframeArea(ui.sandboxScrollArea);
	sandbox->setZoom(2);
	ui.sandboxScrollArea->setWidget(sandbox);
	connect(ui.clearSandbox, SIGNAL(clicked()), sandbox, SLOT(clear()));
	connect(ui.deleteSandbox, SIGNAL(clicked()), sandbox, SLOT(deleteSelected()));
	connect(sandbox, SIGNAL(keyframeDoubleClick(Keyframe*)), this, SLOT(loadUnloadKeyframe(Keyframe*)));

	// The keyframe editor with all the spin boxes on the top left.
	keyframeEditor = ui.KeyframeEditorArea;
	connect(keyframeEditor, SIGNAL(keyframeDropped(Keyframe*)), this, SLOT(loadKeyframe(Keyframe*)));
    connect(keyframeEditor, SIGNAL(saveRequested()), SLOT(saveKeyframe()));
	connect(ui.alignSpeedSlider, SIGNAL(valueChanged(int)), keyframeEditor, SLOT(setSpeedLimit(int)));
	keyframeEditor->setSpeedLimit(ui.alignSpeedSlider->value());
	keyframeEditor->raise();

	// Keyframe player.
	connect(&keyframePlayer, SIGNAL(finished()), this, SLOT(playerFinished()));
	connect(ui.motionSpeedSlider, SIGNAL(valueChanged(int)), &keyframePlayer, SLOT(setSpeedLimit(int)));
	keyframePlayer.setSpeedLimit(ui.motionSpeedSlider->value());
	//connect(ui.smoothingSlider, SIGNAL(valueChanged(int)), &keyframePlayer, SLOT(setTimeCorrection(int)));
	//keyframePlayer.setTimeCorrection(ui.smoothingSlider->value());

    ui.flashButton->setEnabled(false);
    connect(&robotInterface, SIGNAL(robotConnectionChanged(bool)), ui.flashButton, SLOT(setEnabled(bool)));

    // Load joint configuration
    connect(&jointConfiguration, SIGNAL(changed(JointInfo::ListPtr)), motionSequence, SLOT(setJointConfig(JointInfo::ListPtr)));
    connect(&jointConfiguration, SIGNAL(changed(JointInfo::ListPtr)), sandbox, SLOT(setJointConfig(JointInfo::ListPtr)));
    connect(&jointConfiguration, SIGNAL(changed(JointInfo::ListPtr)), keyframeEditor, SLOT(setJointConfig(JointInfo::ListPtr)));
    connect(&jointConfiguration, SIGNAL(changed(JointInfo::ListPtr)), &robotInterface, SLOT(setJointConfig(JointInfo::ListPtr)));
    connect(&jointConfiguration, SIGNAL(changed(JointInfo::ListPtr)), &joystickControl, SLOT(setJointConfig(JointInfo::ListPtr)));

    if(!jointConfiguration.loadFromFile("calibs/robot.ini"))
    {
        QMessageBox::critical(this, tr("Error"),
            tr("Could not load joint configuration file: %1").arg(jointConfiguration.error())
        );
        exit(2);
    }

	// Robot interface.
    connect(this, SIGNAL(complianceChangeRequested(int)), &robotInterface, SLOT(setComplianceMode(int)));
    connect(&robotInterface, SIGNAL(complianceChanged(int)), SLOT(complianceChanged(int)));
	connect(&robotInterface, SIGNAL(message(QString)), this, SLOT(message(QString)));
	connect(&robotInterface, SIGNAL(robotConnected()), this, SLOT(robotConnected()));
	connect(&robotInterface, SIGNAL(robotDisconnected()), this, SLOT(robotDisconnected()));
	connect(&robotInterface, SIGNAL(robotInitialized()), this, SLOT(robotInitialized()));
	connect(&robotInterface, SIGNAL(motionOut(QHash<QString, double>, QHash<QString, double>)), &joystickControl, SLOT(jointAnglesIn(QHash<QString, double>)));
	connect(&robotInterface, SIGNAL(motionOut(QHash<QString, double>, QHash<QString, double>)), &keyframePlayer, SLOT(jointAnglesIn(QHash<QString, double>)));
    connect(&robotInterface, SIGNAL(playbackFinished()), SLOT(playerFinished()));
	connect(ui.alignSpeedSlider, SIGNAL(valueChanged(int)), &robotInterface, SLOT(setSpeedLimit(int)));
    connect(this, SIGNAL(keyframeTransferRequested(const KeyframePlayerItem*,int)), &robotInterface, SLOT(transferKeyframes(const KeyframePlayerItem*,int)));
    connect(&robotInterface, SIGNAL(keyframeTransferFinished(bool)), SLOT(keyframeTransferFinished(bool)));
    connect(&robotInterface, SIGNAL(playbackStarted()), SLOT(handleConnections()));
	robotInterface.setSpeedLimit(ui.alignSpeedSlider->value());
	robotInterface.start();

	// Sometimes the robot interface manages to connect to the robot before the above qt connection have been made.
	// In this case, trigger the robotConnected() slot manually.
	ui.initButton->setEnabled(false);
	if (robotInterface.isRobotConnected())
	{
		message("ROBOT connected. Please initialize.");
		robotConnected();
	}

	// Joystick control.
	connect(&joystickControl, SIGNAL(joystickConnected()), this, SLOT(joystickConnected()));
	connect(&joystickControl, SIGNAL(joystickDisconnected()), this, SLOT(joystickDisconnected()));
	connect(&joystickControl, SIGNAL(message(QString)), this, SLOT(message(QString)));
    connect(&joystickControl, SIGNAL(buttonPressed(QList<bool>)), this, SLOT(saveKeyframe()));
	connect(ui.alignSpeedSlider, SIGNAL(valueChanged(int)), &joystickControl, SLOT(setSpeedLimit(int)));
	joystickControl.setSpeedLimit(ui.alignSpeedSlider->value());

	// Display a list of the motion files in the file manager.
	fileSystemModel = new QFileSystemModel(this);
	fileSystemModel->setRootPath(QDir::currentPath() + "/motions");
	ui.motionfileList->setModel(fileSystemModel);
	ui.motionfileList->setRootIndex(fileSystemModel->index(QDir::currentPath() + "/motions"));
	connect(ui.motionfileList, SIGNAL(doubleClicked(QModelIndex)), this, SLOT(on_loadButton_clicked()));

    // Setup stiff/off/compliant button group
    QButtonGroup* group = new QButtonGroup(this);
    group->addButton(ui.offButton, HardwareCompliant);
    group->addButton(ui.stiffButton, Stiff);
    connect(group, SIGNAL(buttonClicked(int)), SLOT(setRobotState(int)));

    // Progress bar for flash procedure
    m_flashProgressBar.setWindowTitle(tr("Please wait..."));

	// Now set up the default state.
	robotState = Off;
	framesToGrab = 0;
	framesPerSecond = 0;
    isGrabbing = false;
    ui.offButton->setChecked(true);
    ui.stiffButton->setEnabled(false);
	handleConnections();
}

/*
 * This method handles the connections between the Joystick (Joy), Keyframe Player (KFP),
 * Keyframe Editor (KFE), the loaded Keyframe (KF) and the Robot Interface (RI). Depending on
 * the current state of the motion editor (stiff, compliant or off, is a keyframe
 * loaded or not, is the keyframe player playing or not), joint angle and velocity data are
 * sent between the different objects using Qt's signal and slot mechanism.
 * The loaded keyframe connections are handled separately in the loadKeyrame and unloadKeyframe
 * methods, because they don't depend on any other state and it nicer and cleaner to make those
 * connection when they actually happen.
 */
void IgusMotionEditor::handleConnections()
{
	// First Disconnect everything.
    disconnect(keyframeEditor, SIGNAL(motionOut(QHash<QString, double>, QHash<QString, double>, int)), &robotInterface, SLOT(motionIn(QHash<QString, double>, QHash<QString, double>, int)));
	disconnect(&robotInterface, SIGNAL(motionOut(QHash<QString, double>, QHash<QString, double>)), keyframeEditor, SLOT(setJointAngles(QHash<QString, double>)));
	disconnect(&joystickControl, SIGNAL(joystickOut(QHash<QString, double>)), keyframeEditor, SLOT(joystickIn(QHash<QString, double>)));
    disconnect(&keyframePlayer, SIGNAL(motionOut(QHash<QString, double>, QHash<QString, double>)), &robotInterface, SLOT(motionIn(QHash<QString, double>, QHash<QString, double>)));
	disconnect(&keyframePlayer, SIGNAL(motionOut(QHash<QString, double>, QHash<QString, double>)), keyframeEditor, SLOT(motionIn(QHash<QString, double>, QHash<QString, double>)));
    disconnect(&keyframePlayer, SIGNAL(motionOut(QHash<QString, double>, QHash<QString, double>)), keyframeEditor, SLOT(setJointAngles(QHash<QString, double>)));

	// default:
	// the robot is off (not connected or manually set into the off state.)
	// The KFP sends interpolated keyframes to the keyframe editor to visualize the motion.
	// The Joy is connected to the KFE with minimal carrot. The KFE can be moved with the sliders.
	// If a KF is loaded, it is displayed in the KFE and the sliders and the Joy move the KFE and
	// updates the KF. It's best if no connections are made to the RI so that no non-zero positions
	// are set and sent when the robot is suddenly done initializing.
	if (robotState == Off)
	{
        if (keyframePlayer.isPlaying())
		{
			keyframePlayer.interpolating = true;
			keyframePlayer.velocityAdaption = false;
			connect(&keyframePlayer, SIGNAL(motionOut(QHash<QString, double>, QHash<QString, double>)), keyframeEditor, SLOT(setJointAngles(QHash<QString, double>)), Qt::UniqueConnection);
		}
		else
		{
			connect(&joystickControl, SIGNAL(joystickOut(QHash<QString, double>)), keyframeEditor, SLOT(joystickIn(QHash<QString, double>)), Qt::UniqueConnection);
		}
	}

	// stiff
	// The RI does not stream to the KFE. What for? The robot is mostly in the position of the KFE commands.
	// The KFE streams motions to the RI. Sliders move the robot.
	// The Joy moves the KFE, the KFE streams to the RI with the connection mentioned above.
	// When a KF is loaded, the KFE streams into the KF too. The KF sends the speed changes to the KFE (minor).
	// When the KFP is playing, the KFP streams non interpolated keyframes into the RI. The RI streams into the
	// KFE and the sliders are updated, but no motions are sent. A loaded KF is unloaded, the Joy is disconnected.
	else if (robotState == Stiff)
	{
        if (robotInterface.isPlaying())
        {
			connect(&robotInterface, SIGNAL(motionOut(QHash<QString, double>, QHash<QString, double>)), keyframeEditor, SLOT(setJointAngles(QHash<QString, double>)), Qt::UniqueConnection);
		}
		else
		{
            connect(keyframeEditor, SIGNAL(motionOut(QHash<QString, double>, QHash<QString, double>, int)), &robotInterface, SLOT(motionIn(QHash<QString, double>, QHash<QString, double>, int)), Qt::UniqueConnection);
			connect(&joystickControl, SIGNAL(joystickOut(QHash<QString, double>)), keyframeEditor, SLOT(joystickIn(QHash<QString, double>)), Qt::UniqueConnection);
		}
	}

	// compliant (software or hardware are the same)
	// In this mode it is important that the RI streams into the KFE and the KFE updates the sliders.
	// If a KF is loaded, the KFE (or the RI) streams into the KF.
	// The KFE is not connected to the RI.
	// The Joy is not connected either. Nothing moves the robot except for hands.
	// When playing, the KFP streams interpolated keyframes into the KFE for visualization, but the KFE doesn't stream into the robot.
	else if (robotState == SoftwareCompliant || robotState == HardwareCompliant)
	{
		if (keyframePlayer.isPlaying())
		{
			keyframePlayer.interpolating = true;
			keyframePlayer.velocityAdaption = false;
			connect(&keyframePlayer, SIGNAL(motionOut(QHash<QString, double>, QHash<QString, double>)), keyframeEditor, SLOT(setJointAngles(QHash<QString, double>)), Qt::UniqueConnection);
		}
		else
		{
			connect(&robotInterface, SIGNAL(motionOut(QHash<QString, double>, QHash<QString, double>)), keyframeEditor, SLOT(setJointAngles(QHash<QString, double>)), Qt::UniqueConnection);
		}
	}
}



/*
 * Displays a message in the message box on the gui.
 * Hint: You can use HTML to style your messages.
 */
void IgusMotionEditor::message(QString msg)
{
	ui.messageBox->append(msg);
}


/*
 * This is what should happen when the robot was connected to the serial port.
 */
void IgusMotionEditor::robotConnected()
{
	ui.initButton->setEnabled(true);
}

/*
 * These are the things that need to be done when the serial com to the robot was lost.
 */
void IgusMotionEditor::robotDisconnected()
{
	if (keyframePlayer.isPlaying())
	{
		keyframePlayer.stop();
		playerFinished();
	}

	robotState = Off;
	handleConnections();

	ui.connectionStatusIndicator->setPixmap(robolinkIconGrey);
	//ui.offButton->click();
    ui.stiffButton->setEnabled(false);
	ui.initButton->setEnabled(false);
}

/*
 * These are the things that need to be done when the robot was successfully initialized.
 */
void IgusMotionEditor::robotInitialized()
{
	ui.connectionStatusIndicator->setPixmap(robolinkIconOrange);

	if (keyframePlayer.isPlaying())
	{
		keyframePlayer.stop();
		playerFinished();
	}

    ui.stiffButton->setEnabled(true);
	ui.stiffButton->setChecked(true);

	robotState = Stiff;
	handleConnections();

	keyframeEditor->setJointAngles(keyframeEditor->getJointAngles());
}

/*
 * These are the things that need to be done when the init button was pressed.
 */
void IgusMotionEditor::on_initButton_clicked()
{
	if (!robotInterface.isRobotConnected())
		return;

	robotState = Off;
	handleConnections();

	//ui.offButton->click();
    ui.stiffButton->setEnabled(false);
	ui.connectionStatusIndicator->setPixmap(robolinkIconGrey);
	robotInterface.initializeRobot();
}

void IgusMotionEditor::joystickConnected()
{
	ui.joystickStatusIndicator->setPixmap(joystickIconOrange);
}

void IgusMotionEditor::joystickDisconnected()
{
	ui.joystickStatusIndicator->setPixmap(joystickIconGrey);
}


/*
 * Loads a keyframe into the keyframe editor no matter if it's already loaded or not.
 * Dropped keyframes on the keyframe editor are routed to here.
 */
void IgusMotionEditor::loadKeyframe(Keyframe* kf)
{
	if (keyframePlayer.isPlaying())
	{
		keyframePlayer.stop();
		playerFinished();
	}

    keyframeEditor->loadKeyframe(kf);
}


/*
 * Loads a keyframe into the keyframe editor if not already loaded.
 * Otherwise unloads the keyframe. Double clicked or right clicked
 * keyframes are routed to here.
 */
void IgusMotionEditor::loadUnloadKeyframe(Keyframe* kf)
{
	// If the double clicked keyframe is the loaded keyframe, unload the keyframe.
    if (keyframeEditor->loadedKeyframe() == kf)
        keyframeEditor->unloadKeyframe();

	// Otherwise load the keyframe.
	else
		loadKeyframe(kf);
}

/*
 * Creates a new keyframe in the sandbox from the current Keyframe Editor state.
 */
void IgusMotionEditor::saveKeyframe()
{
	Keyframe* kf = new Keyframe(sandbox);
    connect(&jointConfiguration, SIGNAL(changed(JointInfo::ListPtr)), kf, SLOT(setJointConfig(JointInfo::ListPtr)));
    kf->setJointConfig(jointConfiguration.config());
	kf->setJointAngles(keyframeEditor->getJointAngles());
	kf->setSpeed(keyframeEditor->getSpeed());
	kf->setPause(keyframeEditor->getPause());
    kf->setOutputCommand(keyframeEditor->getOutputCommand());
	sandbox->addKeyframe(kf);
}

/**
 * Switches between robot states (off, stiff, compliant)
 *
 * off (hardware compliant) state:
 *  In this state the robot does not receive any commands from the motion editor,
 *  but the keyframe editor continuously displays the poses received from the robot.
 *
 * stiff state:
 *  Switches the robot into stiff mode. The joints are very stiff, the robot
 *  is actually somewhat dangerous to touch. The stiff mode is the only one that
 *  makes sense for playing a motion sequence.
 */
void IgusMotionEditor::setRobotState(int state)
{
    if(keyframePlayer.isPlaying())
    {
        keyframePlayer.stop();
        playerFinished();
    }

    robotState = (RobotState)state;
    handleConnections();

    if(robotInterface.isRobotInitialized())
    {
        switch(state)
        {
            case HardwareCompliant:
                emit complianceChangeRequested(RobotInterface::hardwareCompliance);
                break;
            case Stiff:
                emit complianceChangeRequested(RobotInterface::noCompliance);
                break;
        }
    }
}

void IgusMotionEditor::complianceChanged(int mode)
{
    ui.stiffButton->setChecked(mode == RobotInterface::noCompliance);
    ui.offButton->setChecked(mode == RobotInterface::hardwareCompliance);
}

/*
 * Starts playing the keyframes in the motion sequence.
 * Attention! If the robot is stiff, it will execute the current motion.
 */
void IgusMotionEditor::on_playButton_clicked()
{
	// Abort if already running.
	if (keyframePlayer.isPlaying())
	{
		keyframePlayer.stop();
        playerFinished();
		return;
	}

    if(robotInterface.isPlaying())
    {
        robotInterface.stopPlaying();
        playerFinished();

        return;
    }

	// No go on no frames to play.
	if (motionSequence->isEmpty())
	{
		ui.playButton->setChecked(false);
		return;
	}

	ui.playButton->setText("Stop");
	ui.loopButton->setEnabled(false);
	//ui.offButton->setEnabled(false);
    ui.stiffButton->setEnabled(false);

    keyframeEditor->unloadKeyframe();
	motionSequence->clearSelection();

	// Start playing.
    keyframePlayer.looped = robotInterface.isRobotConnected(); // µC always gets full sequence
	keyframePlayer.playTheseFrames(motionSequence->getKeyframes());

    // Sequence playback is handled by µC if connected, otherwise KeyframePlayer is started.
    if(robotInterface.isRobotConnected())
        emit keyframeTransferRequested(keyframePlayer.playingList(), RobotInterface::KC_PLAY);
    else
        keyframePlayer.start();

	handleConnections();
}

void IgusMotionEditor::on_flashButton_clicked()
{
    m_flashProgressBar.show();

    // Generate looped motion, as we might need the motion
    // from last to first
    keyframePlayer.looped = true;
    keyframePlayer.playTheseFrames(motionSequence->getKeyframes());

    emit keyframeTransferRequested(keyframePlayer.playingList(), RobotInterface::KC_COMMIT);

    keyframePlayer.looped = false;
}

void IgusMotionEditor::keyframeTransferFinished(bool success)
{
    if(m_flashProgressBar.isVisible())
    {
        // Flash in progress
        if(success)
            QMessageBox::information(this, tr("Success"), tr("Motion sequence flashed successfully"));
        else
            QMessageBox::critical(this, tr("Error"), tr("Error during flash procedure"));

        m_flashProgressBar.hide();
    }
    else if(!success)
    {
        // Playback in progress
        QMessageBox::critical(this, tr("Error"), tr("Error while transfering motion sequence"));
        playerFinished();
    }
}

/*
 * Starts looped playing of the keyframes in the motion sequence.
 */
void IgusMotionEditor::on_loopButton_clicked()
{
	// Abort if already running.
	if (keyframePlayer.isPlaying())
	{
		keyframePlayer.stop();
        playerFinished();

        // This is because usually the KFE gets disconnected and doesn't fully reach the final frame.
        keyframeEditor->setJointAngles(keyframePlayer.txJointAngles);
		return;
	}

    if(robotInterface.isPlaying())
    {
        robotInterface.stopPlaying();
        playerFinished();
        return;
    }

	// No go on no frames to play.
	if (motionSequence->isEmpty())
	{
		ui.loopButton->setChecked(false);
		return;
	}

	ui.loopButton->setText("Stop");
	ui.playButton->setEnabled(false);
	//ui.offButton->setEnabled(false);
    ui.stiffButton->setEnabled(false);

    keyframeEditor->unloadKeyframe();
	motionSequence->clearSelection();

	// Start playing.
	keyframePlayer.looped = true;
	keyframePlayer.playTheseFrames(motionSequence->getKeyframes());

    // Sequence playback is handled by µC if connected, otherwise KeyframePlayer is started.
    if(robotInterface.isRobotConnected())
        emit keyframeTransferRequested(keyframePlayer.playingList(), RobotInterface::KC_LOOP);
    else
        keyframePlayer.start();

	handleConnections();
}

/*
 * Resets the gui when the player is done playing the keyframes.
 */
void IgusMotionEditor::playerFinished()
{
	keyframePlayer.looped = false;

	ui.playButton->setChecked(false);
	ui.playButton->setEnabled(true);
	ui.playButton->setText("Play");
	ui.loopButton->setChecked(false);
	ui.loopButton->setEnabled(true);
	ui.loopButton->setText("Loop");
	ui.offButton->setEnabled(true);

    if (robotInterface.isRobotInitialized())
        ui.stiffButton->setEnabled(true);

    handleConnections();
}


/*
 * Starts a frame grabbing process.
 * First it checks if the grabbing process is already running. If so,
 * then it aborts the process and enables the buttons. Otherwise it
 * disables the buttons, gets the grab parameters and then starts the
 * grabbing thread.
 */
void IgusMotionEditor::on_startGrabButton_clicked()
{
	if (isGrabbing)
	{
		frameGrabberFinished();
		return;
	}

	// Get the grab parameters from the gui.
	bool grabTime_ok, framesPerSecond_ok;
	int fps = ui.framesPerSecondEdit->text().toInt(&framesPerSecond_ok);
	double grabTime = ui.grabTimeEdit->text().toDouble(&grabTime_ok);

	// Start only if the grab parameters are ok.
	if (grabTime_ok && framesPerSecond_ok)
	{
		framesPerSecond = fps;
		framesToGrab = (int)(grabTime * framesPerSecond);

		// Prepare the gui for the frame grabbing thread by blocking the buttons and resetting the progress bar.
		ui.startGrabButton->setText("Stop");
		ui.grabProgressBar->setValue(0);

		// Pipe the joint angle stream from the robot interface into the frame grabber.
		connect(&robotInterface, SIGNAL(motionOut(QHash<QString, double>, QHash<QString, double>)), this, SLOT(grabFrame(QHash<QString, double>, QHash<QString, double>)));

		isGrabbing = true;
	}
}

/*
 * Handles a frame grabber tick triggered by an incoming motion signal from the robot interface.
 */
void IgusMotionEditor::grabFrame(QHash<QString, double> ja, QHash<QString, double> jv)
{
	if (lastFrameGrabbedTime.msecsTo(QTime::currentTime()) > 1000.0/framesPerSecond)
	{
		// Create a new Keyframe from the joint angle data and add it to the grabbed frame area.
		Keyframe* kf = new Keyframe(sandbox);
        connect(&jointConfiguration, SIGNAL(changed(JointInfo::ListPtr)), kf, SLOT(setJointConfig(JointInfo::ListPtr)));
        kf->setJointConfig(jointConfiguration.config());
		kf->setJointAngles(ja);
		double maxSpeed = 0;
		foreach (QString key, jv.keys())
			maxSpeed = qMax(jv[key], maxSpeed);
		kf->setSpeed(qBound(10, int(100.0 * maxSpeed / (0.01*(double)ui.motionSpeedSlider->value()*SERVOSPEEDMAX)), 100));
		sandbox->addKeyframe(kf);

		// Update the progress bar.
		bool grabTime_ok, framesPerSecond_ok;
		int framesPerSecond = ui.framesPerSecondEdit->text().toInt(&framesPerSecond_ok);
		double grabTime = ui.grabTimeEdit->text().toInt(&grabTime_ok);
		if (grabTime_ok && framesPerSecond_ok)
		{
			int totalFramesToGrab = (int)(grabTime * framesPerSecond);
			ui.grabProgressBar->setValue(100*(totalFramesToGrab - framesToGrab)/totalFramesToGrab);
		}

		// Abort condition.
		framesToGrab--;
		if (framesToGrab <= 0)
			frameGrabberFinished();

		lastFrameGrabbedTime = QTime::currentTime();
	}
}

/*
 * Does whatever is needed to do when the frame grabber is done.
 * Resets the gui and fixes the signals.
 */
void IgusMotionEditor::frameGrabberFinished()
{
	disconnect(&robotInterface, SIGNAL(motionOut(QHash<QString, double>, QHash<QString, double>)), this, SLOT(grabFrame(QHash<QString, double>, QHash<QString, double>)));
	ui.grabProgressBar->setValue(100);
	ui.startGrabButton->setChecked(false);
	ui.startGrabButton->setText("Record");
	isGrabbing = false;
}


/*
 * It writes the keyframes in the motion sequence area to a text file.
 */
void IgusMotionEditor::on_saveButton_clicked()
{
	QString filename = ui.filenameEdit->text();

	if (filename.isEmpty())
		return;

	filename.replace(QRegExp("\\.txt$"), "");
	filename = "motions/" + filename + ".txt";

	QFile file(filename);
	file.open(QIODevice::WriteOnly);

	if (!file.isWritable())
	{
		message("<font color=\"red\">Cannot write motion file!</font>");
	}
	else
	{
		QTextStream out(&file);

		Keyframe* kf;
		QList< QPointer<Keyframe> > frames = motionSequence->getKeyframes();
		foreach(kf, frames)
			out << kf->toString();
	}

	file.close();

	message(ui.filenameEdit->text() + " saved.");
}

/*
 * Loads the keyframes from the selected file to the motion sequence.
 */
void IgusMotionEditor::on_loadButton_clicked()
{
	if (ui.motionfileList->currentIndex().isValid() && !fileSystemModel->isDir(ui.motionfileList->currentIndex()))
	{
		motionSequence->loadFile(fileSystemModel->filePath(ui.motionfileList->currentIndex()));

		// remove .txt extension
		QString filename = fileSystemModel->fileName(ui.motionfileList->currentIndex());
		//filename.replace(QRegExp("\\.txt$"), "");
		ui.filenameEdit->setText(filename);
	}
}

/*
 * Deletes the selected file from the file manager.
 */
void IgusMotionEditor::on_deleteFileButton_clicked()
{
	if (ui.motionfileList->currentIndex().isValid())
		fileSystemModel->remove(ui.motionfileList->currentIndex());
}

/*
 * Triggers the interpolation of two keyframes in the sandbox.
 */
void IgusMotionEditor::on_interpolateSandbox_clicked()
{
	sandbox->interpolateSelected((double)(ui.interpolateSandboxSlider->value() - ui.interpolateSandboxSlider->minimum())/(ui.interpolateSandboxSlider->maximum() - ui.interpolateSandboxSlider->minimum()));
}

/*
 * Triggers the interpolation of two keyframes in the motion sequence.
 */
void IgusMotionEditor::on_interpolateMotionSequence_clicked()
{
	motionSequence->interpolateSelected((double)(ui.interpolateMotionSequenceSlider->value() - ui.interpolateMotionSequenceSlider->minimum())/(ui.interpolateMotionSequenceSlider->maximum() - ui.interpolateMotionSequenceSlider->minimum()));
}

// Displays the motion speed in the motion speed label.
void IgusMotionEditor::on_motionSpeedSlider_valueChanged(int v)
{
	ui.motionSpeedLabel->setText(QString::number(ui.motionSpeedSlider->value()) + "%");
}

// Displays the align speed in the align speed label.
void IgusMotionEditor::on_alignSpeedSlider_valueChanged(int v)
{
	ui.alignSpeedLabel->setText(QString::number(ui.alignSpeedSlider->value()) + "%");
}


/*
 * The main keyboard handler.
 */
void IgusMotionEditor::keyPressEvent(QKeyEvent* event)
{
	// ESC closes the application.
//	if (event->key() == Qt::Key_Escape)
//	{
//		this->close();
//	}

	// The 0 button resets all sliders to zero.
	if (event->key() == Qt::Key_Escape or event->key() == Qt::Key_0)
	{
		keyframeEditor->zeroKeyframe();
	}

	// K toggles interpolation or keyframe mode (for now)
	else if (event->key() == Qt::Key_K)
	{
		keyframePlayer.interpolating = !keyframePlayer.interpolating;

		if (keyframePlayer.interpolating)
			message("Switched to interpolation control.");
		else
			message("Switched to keyframe control.");
	}

	// V toggles velocity adaption (for now)
	else if (!(event->modifiers() & Qt::ControlModifier) && event->key() == Qt::Key_V)
	{
		keyframePlayer.velocityAdaption = !keyframePlayer.velocityAdaption;

		if (keyframePlayer.velocityAdaption)
			message("Velocity adaption is on.");
		else
			message("Velocity adaption is off.");
	}

	// U unloads the loaded keyframe
	else if (event->key() == Qt::Key_U)
	{
        keyframeEditor->unloadKeyframe();
	}

	// P plays the motion sequence.
	else if (event->key() == Qt::Key_P || event->key() == Qt::Key_Space)
	{
		on_playButton_clicked();
	}

	// L loops the motion sequence.
	else if (event->key() == Qt::Key_L)
	{
		on_loopButton_clicked();
	}

	// I triggers the initialization.
	else if (event->key() == Qt::Key_I)
	{
		if (ui.initButton->isEnabled())
			on_initButton_clicked();
	}

	// Alt-Shift-Enter toggles fullscreen mode.
	else if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) && (event->modifiers() & Qt::AltModifier || event->modifiers() & Qt::ShiftModifier))
	{
		setWindowState(windowState() ^ Qt::WindowFullScreen);
	}

	// Enter and Return grabs a frame.
	else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
	{
        saveKeyframe();
	}

	// STRG-S saves the motion sequence.
	else if (event->key() == Qt::Key_S && event->modifiers() & Qt::ControlModifier)
	{
		on_saveButton_clicked();
	}

	// R (Record) starts or stops grabbing.
	else if (event->key() == Qt::Key_R or event->key() == Qt::Key_G)
	{
		on_startGrabButton_clicked();
	}
}

