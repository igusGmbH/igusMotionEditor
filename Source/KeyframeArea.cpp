/*
 * KeyframeArea.cpp
 *
 * The Keyframe Area is a class where many keyframes are collected and laid out in
 * a flow layout wrapped in a scrollable area. The Motion Sequence area and the
 * Sandbox are both KeyframeArea objects.
 *
 * Besides scrolling, the keyframe area offers a number of functionalities for an
 * easier handling of the keyframes stored inside. There is a zooming function,
 * drag and drop, a rubber band for selecting multiple keyframes with the mouse
 * and keyboard actions such as delete and copy and paste.
 *
 * The keyframes are kept in an ordered sequence. Special care is taken that
 * a correct indexing of the frames is always maintained.
 *
 * The keyframe area can handle drag and drop operations and accepts drops by default.
 * It will handle the events to initiate the drag of a keyframe out of the area and it
 * takes care of keyframes dropped on the area. If the drop comes from a foreign source,
 * the keyframe is added. If the drop comes from the same area, the dragged frame
 * will be moved to the place where it was dropped effectively changing the order
 * in the sequence. Drops directly from a text file are also possible.
 *
 * The keyframe area supports a rubber band select operation. Keep the mouse button
 * pressed and draw a rubber band around items you want to select. If you hold CTRL
 * or SHIFT down when releasing the rubber band, the intersected items will be added
 * to the current selection.
 *
 * The DEL key deletes all currently selected keyframes. CTRL-A selects all keyframes.
 * CTRL-C, CTRL-V will trigger a copy and paste operation. And you can use the arrow
 * keys to navigate the keyframes and together with shift you can select and unselect
 * them.
 *
 *  Author: Marcell Missura, missura@ais.uni-bonn.de
 */

#include <QLayoutItem>
#include <QApplication>
#include <QPixmap>
#include <QMimeData>
#include <QUrl>
#include <QHash>
#include <QString>
#include <QRect>
#include <QtDebug>
#include <QTextStream>
#include <QIODevice>
#include <QHashIterator>

#include "KeyframeArea.h"
#include "Keyframe.h"
#include "FlowLayout.h"

KeyframeArea::KeyframeArea(QWidget *parent) :
	QWidget(parent)
{
	setProperty("scrollArea", true); // Important for the right style to be applied.

	// They keyframe area works best with the FlowLayout. Without the rearranged() signal
	// the indexing of the frames would be difficult to maintain.
	FlowLayout* fl = new FlowLayout(this, 10);
	fl->setMargin(5);
	setLayout(fl);

	connect(layout(), SIGNAL(rearranged()), this, SLOT(reindex()));

	rubberBand = new QRubberBand(QRubberBand::Rectangle, this);

	// Default zoom.
	zoomFactor = 1;

	// Drag and drop related stuff.
	setAcceptDrops(true); // This is important for the drag and drop to be enabled.
	dropIndex = 0;
	dropIndicator = new QFrame(this); // Indicates the position where a frame will be dropped.
	dropIndicator->setGeometry(0, 0, 0, 0);
	dropIndicator->setProperty("dropIndicator", true);
	dropIndicator->hide();
}

/*
 * Adds a keyframe to the area.
 */
void KeyframeArea::addKeyframe(Keyframe *kf)
{
	//kf->installEventFilter(this);
	kf->setZoom(zoomFactor);
	layout()->addWidget(kf);
}

/*
 * Inserts a keyframe at a specific position determined by the index.
 */
void KeyframeArea::insertKeyframeAt(int index, Keyframe *kf)
{
	//kf->installEventFilter(this);
	kf->setZoom(zoomFactor);
	(qobject_cast<FlowLayout*> (layout()))->insertWidgetAt(index, kf);
}

/*
 * Moves a keyframe from from to to. Those better be valid indexes or else!
 */
void KeyframeArea::moveKeyframe(int from, int to)
{
	(qobject_cast<FlowLayout*> (layout()))->moveWidget(from, to);
}

/*
 * Returns a pointer to the keyframe with the index or NULL if
 * no such keyframe exists.
 */
QPointer<Keyframe> KeyframeArea::getKeyframeByIndex(int index)
{
	Keyframe* kf;
	QList<Keyframe*> frames = findChildren<Keyframe*> ();
	foreach (kf, frames)
		if(kf->getIndex() == index)
			return kf;

	return NULL;
}

/*
 * Tells you if this area contains that keyframe or not.
 */
bool KeyframeArea::containsKeyframe(Keyframe* keyframe)
{
	Keyframe* kf;
	QList<Keyframe*> frames = findChildren<Keyframe*> ();
	foreach (kf, frames)
		if (kf == keyframe)
			return true;
	return false;
}

/*
 * Tells you if this area contains any keyframes or not.
 */
bool KeyframeArea::isEmpty()
{
	return findChildren<Keyframe*> ().isEmpty();
}


/*
 * Returns a list of pointers to the keyframes in the area.
 */
QList< QPointer<Keyframe> > KeyframeArea::getKeyframes()
{
	// The keyframes are retrieved in a "funny" way instead of with findChildren(),
	// because findChildren() doesn't return the keyframes in the correct order.

	QList< QPointer<Keyframe> > list;
	for (int i = 0; i < layout()->count(); i++)
		list.append((qobject_cast<Keyframe*> (layout()->itemAt(i)->widget())));
	return list;
}

/*
 * Reindexes all the keyframes contained in area.
 * This is a necessary operation when one or more keyframes are moved or deleted.
 */
void KeyframeArea::reindex()
{
	// The keyframes are retrieved in a "funny" way instead of with findChildren(),
	// because findChildren() doesn't return the keyframes in the correct order.
	for (int i = 0; i < layout()->count(); i++)
		(qobject_cast<Keyframe*> (layout()->itemAt(i)->widget()))->setIndex(i+1);
}

/*
 * Clears the area from all keyframes.
 */
void KeyframeArea::clear()
{
	QList<Keyframe*> frames = findChildren<Keyframe*> ();
	for (int i = 0; i < frames.size(); i++)
		frames.at(i)->deleteLater();
}


/*
 * Unselects all keyframes in the area.
 */
void KeyframeArea::clearSelection()
{
	Keyframe* kf;
	QList<Keyframe*> frames = findChildren<Keyframe*> ();
	foreach (kf, frames)
		kf->setSelected(false);
	update();
}

/*
 * Selects a specific keyframe in the area.
 */
void KeyframeArea::selectKeyframe(Keyframe* kfToSelect)
{
	Keyframe* kf;
	QList<Keyframe*> frames = findChildren<Keyframe*> ();
	foreach (kf, frames)
		if(kf == kfToSelect)
			kf->setSelected(true);
}

/*
 * Selects a specific keyframe in the area by index.
 */
void KeyframeArea::selectKeyframeByIndex(int index)
{
	Keyframe* kf;
	QList<Keyframe*> frames = findChildren<Keyframe*> ();
	foreach (kf, frames)
		if(kf->getIndex() == index)
			kf->setSelected(true);
}

/*
 * Deletes all selected keyframes.
 */
void KeyframeArea::deleteSelected()
{
	Keyframe* kf;
	QList<Keyframe*> frames = findChildren<Keyframe*> ();
	foreach (kf, frames)
		if (kf->isSelected())
			kf->deleteLater();
}


/*
 * Emulates a zoom in effect by resizing all contained widgets to a larger size.
 */
void KeyframeArea::zoomIn()
{
	if (zoomFactor == 10)
		return;

	zoomFactor++;

	QList<Keyframe*> frames = findChildren<Keyframe*> ();
	for (int i = 0; i < frames.size(); i++)
		frames.at(i)->zoomIn();
}

/*
 * Emulates a zoom out effect by resizing all contained widgets to a smaller size.
 */
void KeyframeArea::zoomOut()
{
	if (zoomFactor == -2)
		return;

	zoomFactor--;

	QList<Keyframe*> frames = findChildren<Keyframe*> ();
	for (int i = 0; i < frames.size(); i++)
		frames.at(i)->zoomOut();
}

/*
 * Sets the zoom factor.
 */
void KeyframeArea::setZoom(int zoomFactor)
{
	if (zoomFactor < -2 || zoomFactor > 10)
		return;

	this->zoomFactor = zoomFactor;

	QList<Keyframe*> frames = findChildren<Keyframe*> ();
	for (int i = 0; i < frames.size(); i++)
		frames.at(i)->setZoom(zoomFactor);
}

/*
 * Loads the keyframes from the given file and adds them to the area.
 */
void KeyframeArea::loadFile(QString filename)
{
	QFile file(filename);
	file.open(QIODevice::ReadOnly);
	if (file.isReadable())
	{
		QTextStream stream(&file);
		Keyframe* kf;
		while(!stream.atEnd())
		{
			kf = new Keyframe(this);
            connect(this, SIGNAL(jointConfigChanged(JointInfo::ListPtr)), kf, SLOT(setJointConfig(JointInfo::ListPtr)));
            kf->setJointConfig(m_jointConfig);
			kf->fromString(stream.readLine());
			insertKeyframeAt(dropIndex++, kf);
		}
	}
	file.close();
}

/*
 * Generates a new keyframe interpolated between the first two selected Keyframes.
 * The new keyframe is inserted behind the first keyframe and selected.
 * new = (1-alpha) * first + alpha * second
 */
void KeyframeArea::interpolateSelected(double alpha)
{
	Keyframe* firstFrame;
	Keyframe* secondFrame;
	Keyframe* tempFrame;
	bool firstFrameFound = false;
	bool secondFrameFound = false;

	// Find the first two selected keyframes.
	// The keyframes are retrieved in a "funny" way instead of using findChildren(),
	// because findChildren() doesn't return the keyframes in the correct order.
	for (int i = 0; i < layout()->count(); i++)
	{
		tempFrame = (qobject_cast<Keyframe*> (layout()->itemAt(i)->widget()));

		if (tempFrame->isSelected())
		{
			if (!firstFrameFound)
			{
				firstFrame = tempFrame;
				firstFrameFound = true;
			}
			else if (!secondFrameFound)
			{
				secondFrame = tempFrame;
				secondFrameFound = true;
				break;
			}
		}
	}

	if (firstFrameFound && secondFrameFound)
	{
		// Interpolate between the first and second.
		// new = (1-alpha) * first + alpha*second

		QHash<QString, double> interpolatedJointAngles;
		QHashIterator<QString, double> i(firstFrame->jointAngles);
		while (i.hasNext())
		{
			i.next();
			interpolatedJointAngles[i.key()] = (1-alpha) * firstFrame->jointAngles.value(i.key()) + alpha * secondFrame->jointAngles.value(i.key());
		}

		Keyframe* interpolatedKeyframe = new Keyframe(this);
        connect(this, SIGNAL(jointConfigChanged(JointInfo::ListPtr)), interpolatedKeyframe, SLOT(setJointConfig(JointInfo::ListPtr)));
        interpolatedKeyframe->setJointConfig(m_jointConfig);
		interpolatedKeyframe->setJointAngles(interpolatedJointAngles);
		interpolatedKeyframe->setSpeed(secondFrame->getSpeed());
		insertKeyframeAt(firstFrame->getIndex(), interpolatedKeyframe);
		clearSelection();
		selectKeyframe(interpolatedKeyframe);
	}
}


/*
 * Reacts to a drag enter event by validating the keyframe string.
 */
void KeyframeArea::dragEnterEvent(QDragEnterEvent *event)
{
	if (event->mimeData()->hasText() || event->mimeData()->hasUrls())
	{
		dropIndicator->show();
		event->acceptProposedAction();
	}
}


/*
 * While the drag is moving over the area, the dropIndex is determined and the dropIndicator
 * is displayed at the right spot.
 */
void KeyframeArea::dragMoveEvent(QDragMoveEvent *event)
{
	// A bit of a complicated procedure to determine the right dropIndex from the position of the mouse pointer.
	dropIndex = 0;
	if (layout()->count())
	{
		double w = layout()->itemAt(0)->widget()->width();
		double h = layout()->itemAt(0)->widget()->height();
		double s = layout()->spacing();
		double x = event->pos().x();
		double y = event->pos().y();

		// Map the mouse pointer into a one dimensional space.
		int mouseX = (int)(x + (int) ((y - s / 2) / (h + s)) * childrenRect().width());

		// Run a linear search on the one dimensional space to find out the right dropIndex.
		int rel = (int)(w / 2);
		while (mouseX > rel && dropIndex < layout()->count())
		{
			rel += (int)(w + s);
			dropIndex++;
		}

		// Update the location of the drop indicator.
		if (dropIndex > 0)
			dropIndicator->setGeometry((int)(layout()->itemAt(dropIndex-1)->widget()->x() + w + s / 2 -2), layout()->itemAt(dropIndex-1)->widget()->y(), 4, (int)h);
		else
			dropIndicator->setGeometry(0, 0, 4, (int)h);
	}
	else
	{
		dropIndicator->hide();
	}
}

/*
 * Hides the drop indicator bar.
 */
void KeyframeArea::dragLeaveEvent(QDragLeaveEvent*)
{
	dropIndicator->hide();
}

/*
 * Handles the drop event of a drag and drop operation.
 * If the drop is originating from the same area, only the order of the keyframes is
 * switched. If the drop is coming from a different source one or more keyframes will be
 * added at the last position.
 */
void KeyframeArea::dropEvent(QDropEvent *event)
{
	if (event->source() == this)
	{
		// If the drag originated in this area, we fake a copy action so that the keyframes are not deleted.
		event->setDropAction(Qt::CopyAction);

		//But actually we just want to move the keyframes to a different position in the layout.

		// With a little memcopy trickery we obtain pointers to the keyframes that were dropped here.
		Keyframe* kf;
		QByteArray kfPointerList = event->mimeData()->data("keyframe/pointerlist");
		memcpy(&kf, kfPointerList.data(), sizeof(Keyframe*));

		// And now perform the move. It's some fiddle to get the drop index right. If the keyframe gets
		// dropped right next to itself, then it doesn't even need to move. Also it matters if the
		// drop index is to the left of it or to the right of it, because to the right the drop index has
		// to be decremented due to the nature of the move operation.
		if (qAbs(2*kf->getIndex()-1 - 2*dropIndex) > 1)
		{
			if (dropIndex < kf->getIndex())
				moveKeyframe(kf->getIndex()-1, dropIndex);
			else
				moveKeyframe(kf->getIndex()-1, dropIndex-1);
		}

	}
	else
	{
		// The drag comes from somewhere else.
		// We will create new Keyframe objects from the droppings and add them to the area.
		Keyframe* kf;

		if (event->mimeData()->hasFormat("keyframe/pointerlist"))
		{
			event->setDropAction(Qt::MoveAction);

			QByteArray kfPointerList = event->mimeData()->data("keyframe/pointerlist");
			for (int i = 0; i < kfPointerList.size(); i += sizeof(Keyframe*))
			{
				memcpy(&kf, kfPointerList.data()+i, sizeof(Keyframe*));
				Keyframe* keyframe = new Keyframe(this);
                connect(this, SIGNAL(jointConfigChanged(JointInfo::ListPtr)), keyframe, SLOT(setJointConfig(JointInfo::ListPtr)));
                keyframe->setJointConfig(m_jointConfig);
				keyframe->setPause(kf->getPause());
				keyframe->setSpeed(kf->getSpeed());
				keyframe->motionIn(kf->jointAngles);
				keyframe->modelPixmap = kf->modelPixmap;
				insertKeyframeAt(dropIndex++, keyframe);
			}
		}

		// Plain text mime data means that the keyframes were encoded to a string.
		else if (event->mimeData()->hasText())
		{
			event->setDropAction(Qt::MoveAction);

			QStringList keyframeStrings = event->mimeData()->text().split("\n", QString::SkipEmptyParts);
			foreach (QString oneKeyframeString, keyframeStrings)
			{
				kf = new Keyframe(this);
                connect(this, SIGNAL(jointConfigChanged(JointInfo::ListPtr)), kf, SLOT(setJointConfig(JointInfo::ListPtr)));
                kf->setJointConfig(m_jointConfig);
				kf->fromString(oneKeyframeString);
				insertKeyframeAt(dropIndex++, kf);
			}
		}

		// Url mime data means that one or more files were dropped on the keyframe area that hopefully contain keyframes.
		else if (event->mimeData()->hasUrls())
		{
			event->setDropAction(Qt::CopyAction);

			// Each file comes as an url in the mimeData.
			// We can reuse the loadFile() function here.
			QList<QUrl> urls = event->mimeData()->urls();
			foreach (QUrl url, urls)
			{
				QString fileLocator = url.toLocalFile();
				loadFile(fileLocator);
				emit droppedFileName(fileLocator.right(fileLocator.size() - fileLocator.lastIndexOf("/") -1));
			}
		}
	}

	dropIndicator->hide();
	event->accept();
}


/*
 * The mouse press can initiate different things.
 * If it's a left click and it's on a keyframe, then it's the start of a drag.
 * If it's a right click and it's on a keyframe, then it's a potential right mouse load/unload operation.
 * And in any case, it's the start of a rubber band.
 */
void KeyframeArea::mousePressEvent(QMouseEvent *event)
{
	// If the user left clicked on a widget, we may be starting a drag.
	if (event->button() == Qt::LeftButton
		&& childAt(mapFromGlobal(event->globalPos())) != 0)
	{
		// The drag start position is stored in global coordinates, so that it can easily be mapped to any widget.
		dragStartPosition = event->globalPos();
	}

	// If the user right on a widget, it could be a load/unload operation.
	if (event->button() == Qt::RightButton
		&& childAt(mapFromGlobal(event->globalPos())) != 0)
	{
		// The drag start position is stored in global coordinates, so that it can easily be mapped to any widget.
		rightMouseClickStartPostion = event->globalPos();
	}

	// In any case it will be a rubber band.
	rubberBandOrigin = mapFromGlobal(event->globalPos()); // This works for intercepted events as well.
	rubberBand->setGeometry(QRect(rubberBandOrigin, QSize()));
	rubberBand->show();
}

/*
 * Mouse move events can only occur when a mouse button is held down, because mouse tracking is switched off
 * by default. The mouse move does two things. If a maybe drag is already initialized and the mouse moves away
 * a few pixels from the starting position, then a drag and drop operation is started. And the second thing is
 * updating the rubber band.
 */
void KeyframeArea::mouseMoveEvent(QMouseEvent *event)
{
	// If we might be dragging...
	if (!dragStartPosition.isNull())
	{
		// Is it the left mouse button?
		if (!(event->buttons() & Qt::LeftButton))
			return;

		// Did the mouse move far enough from the drag start position?
		if ((event->globalPos() - dragStartPosition).manhattanLength() < QApplication::startDragDistance())
			return;

		// And we better check if we can find a child at a drag position or the program might crash.
		if (!childAt(mapFromGlobal(dragStartPosition)))
			return;

		// Yes, it's a drag.

		// The keyframe under the mouse and all selected keyframes will be converted to a string, so that
		// the whole selection can be dragged out into a file. Also a list of pointers is prepared for
        // application internal drags.
        // Small fix: If the user starts the drag on a non-selected keyframe, do not include the selection
        // in the drag, as it is probably not expected behavior.
		Keyframe* kf;
        Keyframe* draggedKeyframe = 0; // fix gcc warning
		QByteArray draggedKeyframes;
        QString keyframesString;
        bool includeSelection = true;

		// Check each keyframe in the layout if it's under the mouse or selected.
		// In a first sweep only the under mouse keyframe is taken, so that it's at the first position in the drag mime data.
		for (int i = 0; i < layout()->count(); i++)
		{
			kf = (qobject_cast<Keyframe*> (layout()->itemAt(i)->widget()));

			// Problem: underMouse() cannot be used because it stays true after drag and causes bugs.
			// We are using a self implemented under mouse instead.
			if (kf->geometry().contains(mapFromGlobal(dragStartPosition)))
			{
				draggedKeyframe = kf;
				draggedKeyframes.append((char*) &kf, sizeof(Keyframe*));
				keyframesString.append(kf->toString());
			}

            if(!kf->isSelected())
                includeSelection = false;
		}

        if(includeSelection)
        {
            for (int i = 0; i < layout()->count(); i++)
            {
                kf = (qobject_cast<Keyframe*> (layout()->itemAt(i)->widget()));

                if (kf->isSelected() && kf != draggedKeyframe)
                {
                    draggedKeyframes.append((char*) &kf, sizeof(Keyframe*));
                    keyframesString.append(kf->toString());
                }
            }
        }


		// Construct the QMimeData object from the string representation of the keyframes and a pointer to the under mouse keyframe.
		QMimeData *mimeData = new QMimeData;
		mimeData->setText(keyframesString);
		mimeData->setData("keyframe/pointerlist", draggedKeyframes);

		// Create the QDrag object with this area as the source.
		QDrag *drag = new QDrag(this);
		drag->setMimeData(mimeData);
		//drag->setPixmap(QPixmap::grabWindow(QWidget::winId(), draggedKeyframe->x(), draggedKeyframe->y(), draggedKeyframe->width(), draggedKeyframe->height())); // Take a screen shot of the keyframe.
		drag->setPixmap(QPixmap::grabWidget(draggedKeyframe)); // Take a screen shot of the keyframe.
		drag->setHotSpot(draggedKeyframe->mapFrom(this, event->pos()));

        // In case anyone is looking for this:
        //  I found a strange bug where the drag pixmap is cropped to ~ 60x60 pixels.
        //  This is a Qt4 or graphics driver bug that appears in connection with OpenGL
        //  and is tracked at https://bugreports.qt-project.org/browse/QTBUG-1946.

		// Execute the drag. It will return here and indicate which drop action was chosen.
		drag->exec(Qt::CopyAction | Qt::MoveAction, Qt::CopyAction);

		// Reset the drag position.
		dragStartPosition.setX(0);
		dragStartPosition.setY(0);
	}

	// If we are rubber banding...
	else
	{
		rubberBand->setGeometry(QRect(rubberBandOrigin, event->pos()).normalized());
		update();
	}
}

/*
 * The mouse release event is only interesting for the rubber band and the right mouse button load/unload.
 * A drag and drop is handled in the drop event. If it's the right mouse button that is released and it's
 * close to the position where the it was clicked, it's a load/unload operation. Otherwise the rubber band
 * is released. All keyframes that are touched by it are selected, or toggled depending on if a control key
 * is being pressed.
 */
void KeyframeArea::mouseReleaseEvent(QMouseEvent* event)
{

	// If the right mouse button was released and it's close to where it was clicked the first time,
	// and it happens to be on a widget too, then we have a right mouse load/unload operation.
	if (event->button() == Qt::RightButton
		&& qAbs(rightMouseClickStartPostion.x() - event->globalPos().x()) + qAbs(rightMouseClickStartPostion.y() - event->globalPos().y()) < 8
		&& childAt(mapFromGlobal(event->globalPos())) != 0)
	{
		// Obtain the keyframe that was "double clicked" on.
		QObject* child = childAt(mapFromGlobal(event->globalPos()));
		if (child)
		{
			while (child->parent() != this)
				child = child->parent();
			Keyframe* kf = qobject_cast<Keyframe*> (child);

			// Send the signal.
			emit keyframeDoubleClick(kf);
		}
	}

	// Otherwise it's the end of a rubber band operation.
	else
	{
		QRect rubberBandRect = rubberBand->rect();
		rubberBandRect.translate(rubberBand->mapToParent(QPoint(0, 0)));

		// We need to fix the case when the mouse didn't move, it was just a click (press and release).
		// In such cases the rubberBandRect is empty and will never intersect. Let's grow it by one pixel.
		if (rubberBandRect.isNull())
		{
			rubberBandRect.setWidth(1);
			rubberBandRect.setHeight(1);
		}

		// Now go through each keyframe in the area, check if they intersect with the rubber band
		// and select them or not.
		Keyframe* kf;
		QList<Keyframe*> frames = findChildren<Keyframe*> ();
		foreach (kf, frames)
		{
			QRect childRect = kf->rect();
			childRect.translate(kf->mapToParent(QPoint(0, 0)));

			if (rubberBandRect.intersects(childRect))
			{
				// CTRL or SHIFT pressed
				if (QApplication::keyboardModifiers() > 0)
					kf->toggleSelected();
				else
					kf->setSelected(true);
			}
			else if (QApplication::keyboardModifiers() == 0)
			{
				kf->setSelected(false);
			}
		}
	}

	// Reset the drag position.
	dragStartPosition.setX(0);
	dragStartPosition.setY(0);

	// Reset the right mouse click start position.
	rightMouseClickStartPostion.setX(0);
	rightMouseClickStartPostion.setY(0);

	rubberBand->hide();
	setFocus();
}

/*
 * A double click on a keyframe will load the keyframe into the
 * keyframe editor. Loading is handled up in the main motion editor,
 * so the double clicked keyframe is emitted as a signal from here.
 */
void KeyframeArea::mouseDoubleClickEvent(QMouseEvent* event)
{
	// Double click events are routed up to the MotionEditor
	// using signals and slots.

	// Obtain the keyframe that was double clicked on.
	QObject* child = childAt(mapFromGlobal(event->globalPos()));
	if (child)
	{
		while (child->parent() != this)
			child = child->parent();
		Keyframe* kf = qobject_cast<Keyframe*> (child);

		// Send the signal.
		emit keyframeDoubleClick(kf);
	}
}



/*
 * Wheel events are triggering the zoom function.
 * All keyframes in the area will be shrinked or enlarged.
 */
void KeyframeArea::wheelEvent(QWheelEvent* event)
{
	// CTRL or SHIFT pressed
	if (QApplication::keyboardModifiers() > 0)
	{
		if (event->delta() > 0) // wheel up
			zoomIn();

		if (event->delta() < 0) // wheel down
			zoomOut();
	}
	else
	{
		event->ignore();
	}
}


/*
 * The keyframe area can also handle keyboard actions.
 * The delete key deletes all currently selected keyframes.
 * CTRL-A selects all keyframes.
 * CTRL-C and CTRL-V is a copy and paste opeartion.
 * The arrow keys combined with SHIFT or CTRL select keyframes
 * in a way that you are used to from using windows.
 */
void KeyframeArea::keyPressEvent(QKeyEvent* event)
{
	// The DEL key deletes all currently selected keyframes.
	if (event->key() == Qt::Key_Delete)
	{
		deleteSelected();
	}

	// CTRL-A or SHIFT-A selects all frames.
	else if (event->key() == Qt::Key_A && QApplication::keyboardModifiers() > 0)
	{
		Keyframe* kf;
		QList<Keyframe*> frames = findChildren<Keyframe*> ();
		foreach (kf, frames)
		{
			kf->setSelected(true);
			kf->update();
		}
	}

	// CTRL-C or SHIFT-C copies all selected frames onto the clipboard.
	else if (event->key() == Qt::Key_C && QApplication::keyboardModifiers() > 0)
	{
		QString framesAsString;
		Keyframe* kf;
		for (int i = 0; i < layout()->count(); i++)
		{
			kf = (qobject_cast<Keyframe*> (layout()->itemAt(i)->widget()));
			if (kf->isSelected())
			{
				framesAsString.append(kf->toString());
			}
		}

		QApplication::clipboard()->setText(framesAsString);
	}

	// CTRL-V or SHIFT-V takes the frames from the clipboard adds them to the area.
	else if (event->key() == Qt::Key_V && QApplication::keyboardModifiers() > 0)
	{
		QStringList keyframeStrings = QApplication::clipboard()->text().split("\n", QString::SkipEmptyParts);
		Keyframe* kf;
		foreach (QString oneKeyframeString, keyframeStrings)
		{
			kf = new Keyframe(this);
            connect(this, SIGNAL(jointConfigChanged(JointInfo::ListPtr)), kf, SLOT(setJointConfig(JointInfo::ListPtr)));
            kf->setJointConfig(m_jointConfig);
			kf->fromString(oneKeyframeString);
			addKeyframe(kf);
		}
	}

	// Plus and minus change the zoom factor.
	else if (event->key() == Qt::Key_Plus)
	{
		zoomIn();
	}
	else if (event->key() == Qt::Key_Minus)
	{
		zoomOut();
	}


	else if (event->key() == Qt::Key_Backspace)
	{
		if (getKeyframeByIndex(1))
			keyframeDoubleClick(getKeyframeByIndex(1));
	}

	else if (event->key() == Qt::Key_Right)
	{
		// The keyframes are retrieved in a "funny" way instead of with findChildren(),
		// because findChildren() doesn't return the keyframes in the correct order.
		QList< QPointer<Keyframe> > list;
		for (int i = 0; i < layout()->count()-1; i++)
		{
			Keyframe* kf = qobject_cast<Keyframe*> (layout()->itemAt(i)->widget());
			if (kf->isLoaded())
			{
				keyframeDoubleClick(qobject_cast<Keyframe*> (layout()->itemAt(i+1)->widget()));
				return;
			}
		}
	}

	else if (event->key() == Qt::Key_Left)
	{
		// The keyframes are retrieved in a "funny" way instead of with findChildren(),
		// because findChildren() doesn't return the keyframes in the correct order.
		QList< QPointer<Keyframe> > list;
		for (int i = 1; i < layout()->count(); i++)
		{
			Keyframe* kf = qobject_cast<Keyframe*> (layout()->itemAt(i)->widget());
			if (kf->isLoaded())
			{
				keyframeDoubleClick(qobject_cast<Keyframe*> (layout()->itemAt(i-1)->widget()));
				return;
			}
		}
	}

	// 1 - 9 loads that keyframe from the motion sequence.
	else if (event->key() == Qt::Key_1 && layout()->count() > 0)
		keyframeDoubleClick(getKeyframeByIndex(1));
	else if (event->key() == Qt::Key_2 && layout()->count() > 1)
		keyframeDoubleClick(getKeyframeByIndex(2));
	else if (event->key() == Qt::Key_3 && layout()->count() > 2)
		keyframeDoubleClick(getKeyframeByIndex(3));
	else if (event->key() == Qt::Key_4 && layout()->count() > 3)
		keyframeDoubleClick(getKeyframeByIndex(4));
	else if (event->key() == Qt::Key_5 && layout()->count() > 4)
		keyframeDoubleClick(getKeyframeByIndex(5));
	else if (event->key() == Qt::Key_6 && layout()->count() > 5)
		keyframeDoubleClick(getKeyframeByIndex(6));
	else if (event->key() == Qt::Key_7 && layout()->count() > 6)
		keyframeDoubleClick(getKeyframeByIndex(7));
	else if (event->key() == Qt::Key_8 && layout()->count() > 7)
		keyframeDoubleClick(getKeyframeByIndex(8));
	else if (event->key() == Qt::Key_9 && layout()->count() > 8)
		keyframeDoubleClick(getKeyframeByIndex(9));

	else
		event->ignore();

}

/*
 * This event filter watches the mouse events on the contained keyframes.
 * This is necessary for the rubber band to work right. Mouse releases
 * are always routed to the widget where the mouse press occurred. Keyframes
 * actually don't care about mouse presses, but if the press occurs on top
 * of a keyframe then the release will also be routed to the keyframe and
 * the rubber band won't work.
 */
bool KeyframeArea::eventFilter(QObject *obj, QEvent *event)
{
	if (event->type() == QEvent::MouseButtonPress)
	{
		QMouseEvent *mouseEvent = static_cast<QMouseEvent *> (event);
		mousePressEvent(mouseEvent);
		return true;
	}
	else if (event->type() == QEvent::MouseButtonRelease)
	{
		QMouseEvent *mouseEvent = static_cast<QMouseEvent *> (event);
		mouseReleaseEvent(mouseEvent);
		return true;
	}

	else if (event->type() == QEvent::MouseButtonDblClick)
	{
		// Double click events are routed up to the MotionEditor
		// using signals and slots.

		// Obtain the keyframe that was double clicked on.
		QMouseEvent *mouseEvent = static_cast<QMouseEvent *> (event);
		QObject* child = childAt(mapFromGlobal(mouseEvent->globalPos()));
		while (child->parent() != this)
			child = child->parent();
		Keyframe* kf = qobject_cast<Keyframe*> (child);

		// Send the signal.
		emit keyframeDoubleClick(kf);
	}

	// Continue standard event processing.
	return QObject::eventFilter(obj, event);
}

void KeyframeArea::setJointConfig(const JointInfo::ListPtr &config)
{
    m_jointConfig = config;
    emit jointConfigChanged(config);
}

