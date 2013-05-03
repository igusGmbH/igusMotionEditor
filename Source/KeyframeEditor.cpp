/*
 * KeyframeEditor.cpp
 *
 * This object allows detailed editing of a keyframe.
 * It's a part of the GUI and has its own Qt ui object that can be changed using the Designer.
 * The keyframe editor is located in the top left corner of the main GUI. It shows a 3D model
 * of the robot and spin boxes and sliders to change the joint angles of the robot. It also has
 * spin boxes for the pause and the speed of the keyframe. The spin boxes, the sliders and the
 * 3D model are synchronized such that if one of them is used to change the joint angles, the
 * others are automatically updated accordingly. The keyframe editor stores its own copy of the
 * joint angle data structure and works only with this copy. With every change, the joint angles
 * are emitted as a signal. When a new keyframe is loaded into the editor, the internal copy is
 * overwritten with new joint angles and the GUI is updated.
 *
 *  Created on: Jan 21, 2009
 *  Author: Marcell Missura, missura@ais.uni-bonn.de
 */
#include "KeyframeEditor.h"
#include "globals.h"

const double KeyframeEditor::degToRad = PI/180.0;
const double KeyframeEditor::radToDeg = 180.0/PI;

KeyframeEditor::KeyframeEditor(QWidget *parent)
    : QGroupBox(parent)
    , m_keyframe(0)
{
	ui.setupUi(this);

	setAcceptDrops(true); // This is important for drag and drops.

    speedLimit = 0;

	// Construct the 3D view of the kinematic model.
	robotView = new RobotView3D(ui.poseFrame);
    QHBoxLayout* vlayout = new QHBoxLayout(ui.poseFrame);
    vlayout->addWidget(robotView);
    vlayout->setSpacing(0);
    vlayout->setMargin(0);

	robotView->setJointAngles(&txJointAngles);
	connect(robotView, SIGNAL(jointAnglesChanged()), this, SLOT(jointAnglesChangedByInternalView()));

    QGridLayout* layout = new QGridLayout(ui.sliderWidget);
    ui.sliderWidget->setLayout(layout);

    // Init output comboBox choices
    for(int i = 0; i < Keyframe::DO_COUNT; ++i)
        ui.outputComboBox->insertItem(i, DIGITAL_OUTPUT_LABELS[i]);

	// These connections "broadcast" the speed and pause of the keyframe whenever they are changed.
	connect(ui.speedSpinBox, SIGNAL(valueChanged(int)), this, SIGNAL(speedChanged(int)));
	connect(ui.pauseSpinBox, SIGNAL(valueChanged(double)), this, SIGNAL(pauseChanged(double)));
    connect(ui.outputComboBox, SIGNAL(currentIndexChanged(int)), SLOT(processOutputCommandChange(int)));

    connect(ui.zeroButton, SIGNAL(clicked()), SLOT(zeroKeyframe()));
    connect(ui.unloadKeyframeButton, SIGNAL(clicked()), SLOT(unloadKeyframe()));
    connect(ui.saveKeyframeButton, SIGNAL(clicked()), SIGNAL(saveRequested()));


	/* Now here comes a little signal and slot trickery. We don't want to write a slot
	 * for every single spinbox and slider. So first we connect the valueChanged
	 * signal of all spinboxes to one spinboxValueChanged signal and then we connect
	 * that one signal to the jointAnglesChangedBySpinbox slot that handles the
	 * change by transferring the values from ALL spinboxes to the sliders and the
	 * loaded keyframe with one sweep. The same mechanism is in place for handling the
	 * sliders. Why not just connect all the valueChanged signals from the spinboxes
	 * directly to the keyframeChangedInternally slot you ask? It's because when a new
	 * keyframe is loaded, the spinboxes need to be disconnected for a bit, then the
	 * keyframe is loaded and then the boxes are reconnected. So by mapping all the
	 * spinbox signals to one general signal we only need to connect and reconnect one
     * single signal. */

	// Connect the one signal to the slot that handles changes of the spinboxes.
    connect(this, SIGNAL(spinboxValueChanged()), this, SLOT(jointAnglesChangedBySpinbox()));

	// Connect the one signal to the slot that handles changes of the sliders.
	connect(this, SIGNAL(sliderValueChanged()), this, SLOT(jointAnglesChangedBySlider()));


    // Initial UI state
    ui.unloadKeyframeButton->setEnabled(false);
}

void KeyframeEditor::loadKeyframe(Keyframe *keyframe)
{
    if(m_keyframe)
        unloadKeyframe();

    m_keyframe = keyframe;
    m_keyframe->setLoaded(true);
    m_keyframe->update();

    connect(keyframe, SIGNAL(speedChanged(int)), SLOT(setSpeed(int)), Qt::UniqueConnection);
    connect(keyframe, SIGNAL(pauseChanged(double)), SLOT(setPause(double)), Qt::UniqueConnection);
    connect(keyframe, SIGNAL(outputCommandChanged(int)), SLOT(setOutputCommand(int)), Qt::UniqueConnection);
    connect(keyframe, SIGNAL(destroyed()), SLOT(unloadKeyframe()), Qt::UniqueConnection);
    setJointAngles(keyframe->jointAngles);
    setSpeed(keyframe->getSpeed());
    setPause(keyframe->getPause());
    setOutputCommand(keyframe->getOutputCommand());

    connect(this, SIGNAL(motionOut(QHash<QString,double>,QHash<QString,double>,int)), keyframe, SLOT(motionIn(QHash<QString,double>)), Qt::UniqueConnection);
    connect(this, SIGNAL(speedChanged(int)), keyframe, SLOT(setSpeed(int)), Qt::UniqueConnection);
    connect(this, SIGNAL(pauseChanged(double)), keyframe, SLOT(setPause(double)), Qt::UniqueConnection);
    connect(this, SIGNAL(outputCommandChanged(int)), keyframe, SLOT(setOutputCommand(int)), Qt::UniqueConnection);

    ui.unloadKeyframeButton->setEnabled(true);
}

void KeyframeEditor::unloadKeyframe()
{
    ui.unloadKeyframeButton->setEnabled(false);

    if(!m_keyframe)
        return;

    m_keyframe->disconnect(this);
    disconnect(m_keyframe);
    m_keyframe->setLoaded(false);
    m_keyframe->update();
    m_keyframe = 0;
}

/*
 * This method will transfer the currently loaded joint angles
 * into the spin boxes and sliders that are visible on the GUI.
 */
void KeyframeEditor::transferJointAnglesToGuiElements()
{
	/* When spin boxes and sliders are changed programmatically, they emit their valuedChanged()
	 * signal just the same as if they were changed by hand. So the signals need to be suppressed
	 * for a bit. Using a signal and slot trickery, we connected all spinboxes and all sliders to
	 * one signal each that handles value changes. This makes our life easy, because we have to only
	 * disconnect and reconnect one signal for each type of GUI element. */

	disconnect(this, SIGNAL(spinboxValueChanged()), this, SLOT(jointAnglesChangedBySpinbox()));
	disconnect(this, SIGNAL(sliderValueChanged()), this, SLOT(jointAnglesChangedBySlider()));

    QHash<QString, GUIElements>::iterator it;
    for(it = m_guiElements.begin(); it != m_guiElements.end(); ++it)
    {
        const GUIElements& gui = it.value();

        gui.spinBox->setValue(txJointAngles[it.key()] * radToDeg);
        gui.slider->setValue(gui.slider->minimum() + ((gui.spinBox->value() - gui.spinBox->minimum()) / (gui.spinBox->maximum() - gui.spinBox->minimum())) * (gui.slider->maximum() - gui.slider->minimum()));
    }

	connect(this, SIGNAL(spinboxValueChanged()), this, SLOT(jointAnglesChangedBySpinbox()));
	connect(this, SIGNAL(sliderValueChanged()), this, SLOT(jointAnglesChangedBySlider()));
}

/*
 * Loads a set of joint angles into the keyframe editor.
 * This is triggered when a keyframe is double clicked or dropped on the keyframe editor.
 * It's also used to stream received joint angles from the robot interface to the kf editor.
 */
void KeyframeEditor::setJointAngles(QHash<QString, double> ja)
{
	txJointAngles = ja;
	robotView->updateView();

	// Reset the velocities because the joystick might have messed them up.
    foreach (QString key, txJointVelocities.keys())
		txJointVelocities[key] = speedLimit;

    emit motionOut(txJointAngles, txJointVelocities, ui.outputComboBox->currentIndex());

//    qDebug() << "angles" << txJointAngles;
	//	qDebug() << "velocities" << txJointVelocities;
	//	qDebug() << "sent from setJointAngles\n";

	// A hack to improve performance. Updating the spin boxes is a costly operation.
	if (lastTime.msecsTo(QTime::currentTime()) > 100)
	{
		//robotView->updateView();
		transferJointAnglesToGuiElements();
		lastTime = QTime::currentTime();
	}
}


/*
 * Sets all joint angles to zero.
 */
void KeyframeEditor::zeroKeyframe()
{
	// Set all joint angles to zero.
    foreach (QString key, txJointAngles.keys())
		txJointAngles[key] = 0.0;
	robotView->updateView();

	// Reset the velocities because the joystick might have messed them up.
    foreach (QString key, txJointVelocities.keys())
		txJointVelocities[key] = speedLimit;

	transferJointAnglesToGuiElements();

    emit motionOut(txJointAngles, txJointVelocities, ui.outputComboBox->currentIndex());
}

/*
 * The default motion data transfer slot.
 */
void KeyframeEditor::motionIn(QHash<QString, double> pos, QHash<QString, double> vel)
{
	txJointAngles = pos;
	robotView->updateView();

	// A hack to improve performance. Updating the spin boxes is a costly operation.
	if (lastTime.msecsTo(QTime::currentTime()) > 100)
	{
		//robotView->updateView();
		transferJointAnglesToGuiElements();
		lastTime = QTime::currentTime();
	}
}


/*
 * Loads a set of joint angles into the keyframe editor. It doesn't send out any signals.
 * This is triggered when a keyframe is double clicked or dropped on the keyframe editor.
 * It's also used to stream received joint angles from the robot interface to the kf editor.
 */
void KeyframeEditor::joystickIn(QHash<QString, double> joy)
{
	// Minimal carrot algorithm.
	// Like this I can control the arm fairly smooth with minimal carrot distance.
	// This is needed for when the KFE is controlled with the joy.
	double carrot = speedLimit * 1.0/JOYSTICKRATE + 0.006;
    foreach(const JointInfo& joint, *m_jointConfig)
    {
        if(!joy.contains(joint.name))
            continue;

        txJointAngles[joint.name] = qBound(
            joint.lower_limit, txJointAngles[joint.name] + joy[joint.name] * carrot, joint.upper_limit
        );
        if(qAbs(joy[joint.name]) > 0)
            txJointVelocities[joint.name] = qBound(0.0, qAbs(joy[joint.name]) * speedLimit, speedLimit);
    }

//	qDebug() << txJointVelocities;

	robotView->updateView();
    emit motionOut(txJointAngles, txJointVelocities, ui.outputComboBox->currentIndex());

	// A hack to improve performance. Updating the spin boxes is a costly operation.
	if (lastTime.msecsTo(QTime::currentTime()) > 100)
	{
		//robotView->updateView();
		transferJointAnglesToGuiElements();
		lastTime = QTime::currentTime();
	}
}


/*
 * Sets the align speed limit.
 */
void KeyframeEditor::setSpeedLimit(int sl)
{
	speedLimit = qBound(0.0, 0.01 * (double)sl * SERVOSPEEDMAX, SERVOSPEEDMAX);
    foreach (QString key, txJointVelocities.keys())
		txJointVelocities[key] = speedLimit;
}

/*
 * Returns the currently set joint angles.
 */
QHash<QString, double> KeyframeEditor::getJointAngles()
{
	return txJointAngles;
}

/*
 * Returns the currently set speed, which is stored only in the spin box.
 */
int KeyframeEditor::getSpeed()
{
	return ui.speedSpinBox->value();
}

/*
 * Returns the currently set pause, which is stored only in the spin box.
 */
double KeyframeEditor::getPause()
{
	return ui.pauseSpinBox->value();
}

/*
 * Sets the speed. This is only a slot to set the speed from the outside.
 * It's not triggered by a change of the internal spin box.
 */
void KeyframeEditor::setSpeed(int s)
{
	ui.speedSpinBox->blockSignals(true);
	ui.speedSpinBox->setValue(s);
	ui.speedSpinBox->blockSignals(false);
}

/*
 * Sets the pause. This is only a slot to set the pause from the outside.
 * It's not triggered by a change of the internal spin box.
 */
void KeyframeEditor::setPause(double p)
{
	ui.pauseSpinBox->blockSignals(true);
	ui.pauseSpinBox->setValue(p);
	ui.pauseSpinBox->blockSignals(false);
}

void KeyframeEditor::setOutputCommand(int cmd)
{
    //ui.outputComboBox->blockSignals(true);
    ui.outputComboBox->setCurrentIndex(cmd);
    //ui.outputComboBox->blockSignals(false);
}

int KeyframeEditor::getOutputCommand()
{
    return ui.outputComboBox->currentIndex();
}


/*
 * This slot handles changes caused by the user tinkering with the spin boxes
 * and manually adjusting the joint angles. The contents of the spin boxes are
 * transfered into the currently loaded joint angles and the sliders are synchronized.
 */
void KeyframeEditor::jointAnglesChangedBySpinbox()
{
	/* When spin boxes and sliders are changed programmatically, they emit their valuedChanged()
	 * signal just the same as if they were changed by hand. So the signals need to be suppressed
	 * for a bit. Using a signal and slot trickery, we connected all spinboxes and all sliders to
	 * one signal each that handles value changes. This makes our life easy, because we have to only
	 * disconnect and reconnect one signal for each type of GUI element. */

	disconnect(this, SIGNAL(spinboxValueChanged()), this, SLOT(jointAnglesChangedBySpinbox()));
	disconnect(this, SIGNAL(sliderValueChanged()), this, SLOT(jointAnglesChangedBySlider()));

    QHash<QString, GUIElements>::iterator it;
    for(it = m_guiElements.begin(); it != m_guiElements.end(); ++it)
    {
         const GUIElements& gui = it.value();

         gui.slider->setValue(gui.slider->minimum()
                              + (gui.spinBox->value() - gui.spinBox->minimum())
                              * (gui.slider->maximum() - gui.slider->minimum())
                              / (gui.spinBox->maximum() - gui.spinBox->minimum()));
         txJointAngles[it.key()] = gui.spinBox->value() * degToRad;
    }

	connect(this, SIGNAL(spinboxValueChanged()), this, SLOT(jointAnglesChangedBySpinbox()));
	connect(this, SIGNAL(sliderValueChanged()), this, SLOT(jointAnglesChangedBySlider()));

	robotView->updateView();
    emit motionOut(txJointAngles, txJointVelocities, ui.outputComboBox->currentIndex());
}


/*
 * This slot handles changes caused by the user tinkering with the sliders
 * and manually adjusting the joint angles. The contents of the sliders are
 * transfered into the currently loaded joint angles and the spin boxes are synchronized.
 */
void KeyframeEditor::jointAnglesChangedBySlider()
{
	/* When spin boxes and sliders are changed programmatically, they emit their valuedChanged()
	 * signal just the same as if they were changed by hand. So the signals need to be suppressed
	 * for a bit. Using a signal and slot trickery, we connected all spinboxes and all sliders to
	 * one signal each that handles value changes. This makes our life easy, because we have to only
	 * disconnect and reconnect one signal for each type of GUI element. */

	disconnect(this, SIGNAL(spinboxValueChanged()), this, SLOT(jointAnglesChangedBySpinbox()));
	disconnect(this, SIGNAL(sliderValueChanged()), this, SLOT(jointAnglesChangedBySlider()));

    QHash<QString, GUIElements>::iterator it;
    for(it = m_guiElements.begin(); it != m_guiElements.end(); ++it)
    {
        const GUIElements& gui = it.value();

        gui.spinBox->setValue(gui.spinBox->minimum()
            + (((double)gui.slider->value() - gui.slider->minimum())
                       / (gui.slider->maximum() - gui.slider->minimum()))
                              * (gui.spinBox->maximum() - gui.spinBox->minimum()));

        txJointAngles[it.key()] = gui.spinBox->value () * degToRad;
    }

	connect(this, SIGNAL(spinboxValueChanged()), this, SLOT(jointAnglesChangedBySpinbox()));
	connect(this, SIGNAL(sliderValueChanged()), this, SLOT(jointAnglesChangedBySlider()));

    robotView->updateView();

    emit motionOut(txJointAngles, txJointVelocities, ui.outputComboBox->currentIndex());
}

void KeyframeEditor::processOutputCommandChange(int oc)
{
    emit outputCommandChanged(oc);
    emit motionOut(txJointAngles, txJointVelocities, oc);
}

/*
 * This slot handles changes to the loaded keyframe caused by
 * the user grabbing and moving the internal 3D view.
 */
void KeyframeEditor::jointAnglesChangedByInternalView()
{
	transferJointAnglesToGuiElements();
    emit motionOut(txJointAngles, txJointVelocities, ui.outputComboBox->currentIndex());
}

/*
 * Verifies the keyframe string syntax and accepts keyframe drags if the check is positive.
 */
void KeyframeEditor::dragEnterEvent(QDragEnterEvent *event)
{
	if (event->mimeData()->hasFormat("text/plain")
		&& Keyframe::validateString(event->mimeData()->text()))
		event->acceptProposedAction();
}

/*
 * Handles the drop event of a drag and drop operation.
 * It's a bit strange, because the dropped keyframe is signaled back out and the main
 * MotionEditor will handle the loading of a keyframe. The reason for this "funny" is that
 * the dropped keyframe needs to be loaded into the robot interface as well and the keyframe
 * editor shouldn't have to know about the robot interface just for this purpose.
 */
void KeyframeEditor::dropEvent(QDropEvent *event)
{
	// With a little memcpy magic we obtain a pointer to the keyframe that was
	// dropped here. In case multiple frames are dropped at once, only the pointer
	// to the first one is taken.
	Keyframe* kf;
	memcpy(&kf, event->mimeData()->data("keyframe/pointerlist").data(), sizeof(Keyframe*));
	emit keyframeDropped(kf);

	// Respond to the event by indicating the right drop action.
	// All drags onto the editor are only copy actions.
	event->setDropAction(Qt::CopyAction);
	event->accept();
}

void KeyframeEditor::setJointConfig(const JointInfo::ListPtr &config)
{
    QGridLayout* layout = qobject_cast<QGridLayout*>(ui.sliderWidget->layout());
    m_jointConfig = config;

    foreach(QObject* w, ui.sliderWidget->findChildren<QWidget*>())
        delete w;

    m_guiElements.clear();
    txJointVelocities.clear();

    // Create the sliders & spinboxes
    for(int i = 0; i < config->size(); ++i)
    {
        const JointInfo& joint = config->at(i);
        GUIElements elem;
        QLabel* label = new QLabel(joint.name, ui.sliderWidget);
        elem.slider = new QSlider(Qt::Horizontal, ui.sliderWidget);
        elem.spinBox = new QDoubleSpinBox(ui.sliderWidget);
        QLabel* degLabel = new QLabel("°", ui.sliderWidget);

        elem.spinBox->setMaximum(joint.upper_limit * radToDeg);
        elem.spinBox->setMinimum(joint.lower_limit * radToDeg);
        elem.spinBox->setValue(0);

        elem.slider->setMinimum(-1000);
        elem.slider->setMaximum(1000);
        elem.slider->setValue(0);

        connect(elem.spinBox, SIGNAL(valueChanged(double)), this, SIGNAL(spinboxValueChanged()));
        connect(elem.slider, SIGNAL(valueChanged(int)), this, SIGNAL(sliderValueChanged()));

        layout->addWidget(label, i, 0);
        layout->addWidget(elem.slider, i, 1);
        layout->addWidget(elem.spinBox, i, 2);
        layout->addWidget(degLabel, i, 3);

        m_guiElements[joint.name] = elem;

        txJointVelocities[joint.name] = speedLimit;
    }

    robotView->setJointConfig(config);
    jointAnglesChangedByInternalView();
}

