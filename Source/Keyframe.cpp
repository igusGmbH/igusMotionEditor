/*
 * Keyframe.cpp
 *
 * This class is a combination of a pose of the robot described by joint angles
 * and its graphical representation.
 *
 * The joint angles are doubles in rad packed in a QHash with associated names as hash keys.
 * The associative representation was chosen as a base for more explicit algorithms, that assign
 * joint angles very clearly by names instead of just iterating through a list. I hope this will
 * yield robustness, as in inserting or removing a joint from the list would destroy the algorithms
 * everywhere in the program, where the joint angles are visited.
 *
 * The graphical representation is a subclass of QFrame and a 3D display of the robot in
 * the given pose. The 3D model is converted the a pixmap that can be drawn much faster.
 *
 *  Created on: Jan 14, 2009
 *  Author: Marcell Missura, missura@ais.uni-bonn.de
 */

#include <QtGui>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QString>
#include <QStringList>
#include <QHash>
#include <QMouseEvent>
#include <QSizePolicy>
#include <QSize>
#include <QFont>
#include <QHashIterator>
#include <QRegExp>
#include <QPaintEvent>
#include <QPainter>
#include <QSpinBox>
#include <QDebug>

#include "Keyframe.h"
#include "RobotView3D.h"

const char* DIGITAL_OUTPUT_LABELS[] = {
    "-",     // DO_IGNORE
    "set",   // DO_SET
    "reset"  // DO_RESET
};

Keyframe::Keyframe(QWidget *parent) :
	QWidget(parent)
{
	// Set the guidelines for the size. This influences how the keyframes behave in a layout when the window is resized.
    setFixedSize(120, 200);

	/*
	 * When you use a layout, you do not need to pass a parent when constructing the child widgets.
	 * The layout will automatically reparent the widgets so that they are children of the widget
	 * on which the layout is installed. Widgets in a layout are children of the widget on which
	 * the layout is installed, not of the layout itself. Widgets can only have other widgets as
	 * parent, not layouts.
	 */

	index = 0;
	pause = 0.0;
	speed = 50;
	selected = false;
	loaded = false;
    ignoreMouse = true;

    robotViewContainer = new QLabel(this);
    robotViewContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    QHBoxLayout* rvl = new QHBoxLayout(robotViewContainer);
    rvl->setContentsMargins(0, 0, 0, 0);

    robotView = new RobotView3D(robotViewContainer);
	robotView->setJointAngles(&jointAngles);
	robotView->ignoreMouse = true;
    robotView->hide();

    rvl->addWidget(robotView);

	QLabel *label = new QLabel;
	label->setText("#");

	indexLabel = new QLabel;
	indexLabel->setNum(index);

	QPushButton *deleteButton = new QPushButton;
	deleteButton->setProperty("frameDeleteButton", true);
	deleteButton->setMaximumHeight(12);
	deleteButton->setMinimumWidth(12);
	deleteButton->setText("x");
	connect(deleteButton, SIGNAL(clicked()), this, SLOT(deleteLater()));

	QLabel* speedLabel = new QLabel;
	speedLabel->setText("speed:");
	speedLabel->setMaximumHeight(13);

	QLabel* pauseLabel = new QLabel;
	pauseLabel->setText("pause:");
    pauseLabel->setMaximumHeight(13);

    QLabel* digitalLabel = new QLabel("out:");
    digitalLabel->setMaximumHeight(13);

    digBox = new QComboBox;
    for(int i = 0; i < DO_COUNT; ++i)
        digBox->insertItem(i, DIGITAL_OUTPUT_LABELS[i]);
    connect(digBox, SIGNAL(currentIndexChanged(int)), SIGNAL(outputCommandChanged(int)));

	speedBox = new QSpinBox;
	speedBox->setProperty("keyframeSpinBox", true);
	speedBox->setAccelerated(true);
	//speedBox->setKeyboardTracking(false);
	speedBox->setAlignment(Qt::AlignRight);
	speedBox->setRange(1, 100);
	speedBox->setValue(speed);
	speedBox->setMaximumWidth(50);
	speedBox->setMaximumHeight(15);
    speedBox->setSuffix("%");
	connect(speedBox, SIGNAL(valueChanged(int)), this, SLOT(speedChangedBySpinbox()));

	pauseBox = new QDoubleSpinBox;
	pauseBox->setProperty("keyframeSpinBox", true);
	pauseBox->setAccelerated(true);
	//pauseBox->setKeyboardTracking(false);
	pauseBox->setAlignment(Qt::AlignRight);
	pauseBox->setRange(0, 1000);
	pauseBox->setValue(pause);
	pauseBox->setMaximumWidth(50);
	pauseBox->setMaximumHeight(15);
    pauseBox->setSuffix("s");
	connect(pauseBox, SIGNAL(valueChanged(double)), this, SLOT(pauseChangedBySpinbox()));


	QHBoxLayout *headerLayout = new QHBoxLayout;
	headerLayout->setContentsMargins(2, 4, 2, 3);
	headerLayout->setSpacing(0);
	headerLayout->addWidget(label);
	headerLayout->addWidget(indexLabel);
	headerLayout->addStretch(1);
	headerLayout->addWidget(deleteButton);

	QGridLayout *footerLayout = new QGridLayout;
    footerLayout->setContentsMargins(1, 1, 1, 3);
    footerLayout->setSpacing(0);
    footerLayout->setVerticalSpacing(3);

	footerLayout->addWidget(speedLabel, 0, 0, Qt::AlignRight);
    footerLayout->addWidget(speedBox, 0, 1);

	footerLayout->addWidget(pauseLabel, 1, 0, Qt::AlignRight);
    footerLayout->addWidget(pauseBox, 1, 1);

    footerLayout->addWidget(digitalLabel, 2, 0, Qt::AlignRight);
    footerLayout->addWidget(digBox, 2, 1);

	QVBoxLayout *layout = new QVBoxLayout;
    layout->setContentsMargins(3, 0, 3, 0);
	layout->setSpacing(0);
	layout->addLayout(headerLayout);
    layout->addWidget(robotViewContainer);
	robotView->hide();
    layout->addLayout(footerLayout);

	setLayout(layout);
}

Keyframe::~Keyframe()
{
	if (robotView)
		robotView->deleteLater();
}

/*
 * Sets the index of the keyframe.
 * The index of the keyframe represents the position of the keyframe in a layout.
 * More specifically, the index is the position of the keyframe in the motion
 * sequence that is being created in the motion editor area.
 */
void Keyframe::setIndex(int index)
{
	this->index = index;
	indexLabel->setNum(index);
}

/*
 * Sets the pause of the keyframe.
 * The pause is the amount of ticks that the robot will wait after reaching this keyframe.
 */
void Keyframe::setPause(double pause)
{
	this->pause = pause;
	pauseBox->blockSignals(true);
	pauseBox->setValue(pause);
	pauseBox->blockSignals(false);
}

/*
 * Returns the pause of the keyframe.
 * The pause is the amount of ticks that the robot will wait after reaching this keyframe.
 */
double Keyframe::getPause()
{
	return this->pause;
}


/*
 * Sets the speed of the keyframe.
 * The speed parameter is a percental value (1 - 100) that describes how fast this keyframe
 * should be reached. When the speed is 100, the robot will try to reach the keyframe as fast
 * as possible. A speed of 1 is really slow. Setting 0 as speed is not recommended, because
 * the robot will not move at all and the program will freeze while playing the keyframes.
 */
void Keyframe::setSpeed(int speed)
{
	this->speed = speed;
	speedBox->blockSignals(true);
	speedBox->setValue(speed);
	speedBox->blockSignals(false);
}

void Keyframe::setOutputCommand(int cmd)
{
    digBox->blockSignals(true);
    digBox->setCurrentIndex((int)cmd);
    digBox->blockSignals(false);
}

Keyframe::DigitalOutput Keyframe::getOutputCommand() const
{
    return (DigitalOutput)digBox->currentIndex();
}

/*
 * A slot for handling the internal speed spin box.
 */
void Keyframe::speedChangedBySpinbox()
{
	setSpeed(speedBox->value());
	emit speedChanged(speedBox->value());
}


/*
 * A slot for handling the internal pause spin box.
 */
void Keyframe::pauseChangedBySpinbox()
{
	setPause(pauseBox->value());
	emit pauseChanged(pauseBox->value());
}

/*
 * Returns the speed of the keyframe.
 * The speed parameter is a percental value (1 - 100) that describes how fast this keyframe
 * should be reached. When the speed is 100, the robot will try to reach the keyframe as fast
 * as possible. A speed of 1 is really slow. Setting 0 as speed is not recommended, because
 * the robot will not move at all and the program will freeze while playing the keyframes.
 */
int Keyframe::getSpeed()
{
	return this->speed;
}


/*
 * Returns the index of the keyframe.
 * The index of the keyframe represents the position of the keyframe in a layout.
 * More specifically, the index is the position of the keyframe in the motion
 * sequence that is being created in the motion editor area.
 */
int Keyframe::getIndex()
{
	return this->index;
}

/**
 * Updates the 3D view
 */
void Keyframe::updateView()
{
    if(robotView)
    {
        robotView->updateView();
        modelPixmap = robotView->getPixmap(robotViewContainer->width(), robotViewContainer->height());
        robotViewContainer->setPixmap(modelPixmap);
    }
}

/*
 * Sets the joint angles if this keyframe.
 * It also renders the pixmap, so it's expensive.
 */
void Keyframe::setJointAngles(const QHash<QString, double> ja)
{
	jointAngles = ja;
    updateView();
}

/*
 * A slot for a motion stream input. It updates the 3D model, but does
 * not render the pixmap, because during streaming only the 3D model is
 * shown. The pixmap is rendered once when the keyframes is unloaded.
 */
void Keyframe::motionIn(QHash<QString, double> angles)
{
	jointAngles = angles;

	if (robotView)
        robotView->updateView();

	update();
}

// Replaces the current pixmap with a new one generated from the 3D model
// with the currently set joint angles.
void Keyframe::updatePixmap()
{
    updateView();
	update();
}


/*
 * Returns the distance between this and the other keyframe.
 * The distance is calculated as the maximum norm on the joint angles.
 */
double Keyframe::distance(Keyframe* kf)
{
	// Find the maximum joint distance.
	double distance = 0;
	QHashIterator<QString, double> jointIterator(jointAngles);
	while (jointIterator.hasNext())
	{
		jointIterator.next();

		if (qAbs(this->jointAngles[jointIterator.key()] - kf->jointAngles[jointIterator.key()]) > distance)
			distance = qAbs(this->jointAngles[jointIterator.key()] - kf->jointAngles[jointIterator.key()]);
	}

	return distance;
}

/*
 * Returns the distance between this and the "other" keyframe.
 * The distance is calculated as the maximum norm on the joint angles.
 */
double Keyframe::distance(QHash<QString, double> ja)
{
	// Find the maximum joint distance.
	double distance = 0;
	QHashIterator<QString, double> jointIterator(jointAngles);
	while (jointIterator.hasNext())
	{
		jointIterator.next();

		if (qAbs(this->jointAngles[jointIterator.key()] - ja[jointIterator.key()]) > distance)
			distance = qAbs(this->jointAngles[jointIterator.key()] - ja[jointIterator.key()]);
	}

	return distance;
}

/*
 * Broadcasts the new joint angles in case they were changed by the internal view
 * by the user grabbing the limbs of the skeleton and moving them with the mouse.
 */
void Keyframe::jointAnglesChangedByInternalView()
{
	emit jointAnglesChanged(jointAngles);
}

/*
 * Converts a Keyframe to a string representation.
 * The index of the keyframe is not included on
 * purpose, because the index is the position of the frame in a
 * layout or in a sequence. When the frame is converted to a string,
 * it means that it's being dragged away or exported somehow, so
 * it's taken away from its sequence and the index becomes invalid.
 */
const QString Keyframe::toString()
{
	QString string;

	string.append("speed:" + QString::number(this->speed));
	string.append(" pause:" + QString::number(this->pause));
    string.append(" output:" + QString::number(digBox->currentIndex()));

	QHashIterator<QString, double> i(jointAngles);
	while (i.hasNext())
	{
		i.next();
		string.append(" " + i.key() + ":" + QString::number(i.value()));
	}

	string.append("\n");

	return string;
}

/*
 * Overwrites the joint angles in this Keyframe with the angles
 * provided by the string representation. If the string is not
 * in a valid format who knows what will happen.
 */
void Keyframe::fromString(QString keyframeString)
{
	// Don't do anything if the string is not valid.
	if (!validateString(keyframeString))
		return;

	// Chop off everything starting from the first newline character.
	if (keyframeString.contains("\n"))
		keyframeString.truncate(keyframeString.indexOf("\n"));

	// Split the string.
	QStringList keyframeParts = keyframeString.split(QRegExp("\\s"), QString::SkipEmptyParts);
	QString part;
	QStringList partBits;
	bool ok = true;

	foreach (part, keyframeParts)
	{
		partBits = part.split(":");
        qDebug() << partBits;

		if (partBits.at(0) == "speed")
			setSpeed(partBits.at(1).toInt(&ok));

		else if (partBits.at(0) == "pause")
			setPause(partBits.at(1).toDouble(&ok));

        else if (partBits.at(0) == "output")
            setOutputCommand((DigitalOutput)partBits.at(1).toInt(&ok));

		else
			this->jointAngles[partBits.at(0)] = partBits.at(1).toDouble(&ok);

		if (!ok)
			qDebug() << "When creating a keyframe from string, could not extract value from " << part;
	}

    updateView();
}


/*
 * Converts the string representation to a joint angle hash.
 */
QHash<QString, double> Keyframe::jointAnglesFromString(QString keyframeString)
{
	QHash<QString, double> ja;

	// Chop off everything starting from the first newline character.
	if (keyframeString.contains("\n"))
		keyframeString.truncate(keyframeString.indexOf("\n"));

	// Split the string.
	QStringList keyframeParts = keyframeString.split(QRegExp("\\s"), QString::SkipEmptyParts);
	QString part;
	QStringList partBits;
	bool ok = true;

	foreach (part, keyframeParts)
	{
		if (!part.contains(":"))
		{
			qDebug() << "Invalid keyframe string";
			return ja;
		}

		partBits = part.split(":");

        if (partBits.at(0) != "speed" && partBits.at(0) != "pause" && partBits.at(0) != "output")
			ja[partBits.at(0)] = partBits.at(1).toDouble(&ok);

		if (!ok)
			qDebug() << "When creating a keyframe from string, could not extract value from " << part;
	}

	return ja;
}

/*
 * Validates if a keyframe string representation is in a valid format.
 */
bool Keyframe::validateString(QString keyframeString)
{
	// Either you are a Perl programmer, or you gonna have to read up on regular expressions to understand this line.
    QRegExp rx = QRegExp("^((speed:\\d{1,3})?(\\s)?(pause:\\d{1,3})?((\\s)?[\\w\\(\\)]{1,}:-?\\d{1,}(\\.\\d{1,})?)*\n?){1,}$");
	return rx.exactMatch(keyframeString);
}

/*
 * Maintains the selected property which is then used to determine, if the frame
 * should be deleted when the DEL button is pressed and also the style sheet
 * reacts to it to visualize selected frames.
 */
void Keyframe::setSelected(bool flag)
{
	if (flag)
		selected = true;
	else
		selected = false;
}

/*
 * Toggles the selected property which is then used to determine, if the frame
 * should be deleted when the DEL button or the mirror button is pressed.
 */
void Keyframe::toggleSelected()
{
	selected = !selected;
}

/*
 * Tells you if the frame is selected or not.
 */
bool Keyframe::isSelected()
{
	return selected;
}

/*
 * Notifies the keyframe of being loaded into the keyframe editor or not.
 * The keyframe will then draw itself with a thick border if loaded and show
 * or hide the 3D model widget. In a loaded state the OpenGL 3D model widget
 * is shown and a thick border is drawn around the keyframe. When not loaded,
 * there is no special border indication and the 3D is hidden and replaced
 * with a pixmap instead.
 */
void Keyframe::setLoaded(bool flag)
{
	if (flag)
	{
		loaded = true;
		if (robotView)
			robotView->show();
	}
	else
	{
        loaded = false;
        updateView();
		if (robotView)
			robotView->hide();
	}
}

/*
 * Tells you if this is the currently loaded frame or not.
 */
bool Keyframe::isLoaded()
{
	return loaded;
}

/*
 * Emulates a zoom effect by resizing the widget to a larger size.
 */
void Keyframe::zoomIn()
{
	QSize size = minimumSize();
	setFixedSize(size + QSize(20, 20*size.height()/size.width()));
}

/*
 * Emulates a zooming out effect by resizing the widget to a smaller size.
 */
void Keyframe::zoomOut()
{
	if (width() > 60) // absolute minimum size: 60, 90
	{
		QSize size = minimumSize();
		setFixedSize(size - QSize(20, 20*size.height()/size.width()));
	}
}


/*
 * Emulates a zoom effect by resizing the widget.
 */
void Keyframe::setZoom(int zoomFactor)
{
	if (zoomFactor > -3 && zoomFactor < 10)
	{
		QSize size = minimumSize();
		setFixedSize(QSize(60,90) + zoomFactor*QSize(20, 20*size.height()/size.width()));
	}
}


void Keyframe::mouseMoveEvent(QMouseEvent* e)
{
	if (ignoreMouse)
		e->ignore();
}

void Keyframe::mousePressEvent(QMouseEvent* e)
{
	if (ignoreMouse)
		e->ignore();
}

void Keyframe::mouseReleaseEvent(QMouseEvent* e)
{
	if (ignoreMouse)
		e->ignore();
}

void Keyframe::mouseDoubleClickEvent(QMouseEvent* e)
{
	if (ignoreMouse)
		e->ignore();
}

void Keyframe::keyPressEvent(QKeyEvent* event)
{
	// Currently this method is implemented for the sole purpose
	// of "eating" enter key presses that come from the spin boxes.
	// For some reason, the spin boxes don't consume enter key presses
	// like they should.
}



/*
 * I would prefer to do all the styling of the keyframe with style sheets,
 * but for some reason they are drawn by hand.
 */
void Keyframe::paintEvent(QPaintEvent*)
{
	QPainter painter(this);
	QPen pen;

	QColor white(255, 255, 255);
	QColor lightGrey(230, 230, 230);
	QColor darkGrey(153, 153, 153);
	QColor darkerGrey(75, 75, 75);
	QColor igusOrange(255, 153, 0);
	QColor darkRed(125, 0, 0);

    if(modelPixmap.size() != robotViewContainer->size())
        updateView();

    // Draw the header and the footer ornaments.
    if (isSelected())
    {
        pen.setColor(darkGrey);
        painter.setBrush(darkGrey);
    }
    else
    {
        pen.setColor(lightGrey);
        painter.setBrush(lightGrey);
    }

	// Clear everything with white.
    painter.fillRect(0, 0, width(), height(), isSelected() ? darkGrey : lightGrey);

	// Draw the frame border.
	// Draw a highlighted border if the keyframe is loaded.
	painter.setBrush(Qt::NoBrush);
	if (isLoaded())
	{
		int borderWidth = 3;
		pen.setWidth(borderWidth);
		pen.setColor(igusOrange);
		painter.setPen(pen);
		painter.drawRect(1, 1, this->width()-borderWidth, this->height()-borderWidth);
	}
	else
	{
		int borderWidth = 1;
		pen.setWidth(borderWidth);
		pen.setColor(darkGrey);
		painter.setPen(pen);
		painter.drawRect(0, 0, this->width()-borderWidth, this->height()-borderWidth);
	}
}

void Keyframe::setJointConfig(const JointInfo::ListPtr& config)
{
    robotView->setJointConfig(config);
}
