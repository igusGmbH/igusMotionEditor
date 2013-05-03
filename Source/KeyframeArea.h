/*
 * KeyframeArea.h
 *
 *  Created on: Jan 23, 2009
 *  Author: Marcell Missura, missura@ais.uni-bonn.de
 */
#ifndef KEYFRAMEAREA_H_
#define KEYFRAMEAREA_H_

#include <QtGui>
#include <QString>
#include <QPoint>
#include <QPointer>
#include <QLine>
#include <QRubberBand>
#include <QList>
#include <QEvent>
#include <QKeyEvent>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDragLeaveEvent>
#include <QDropEvent>
#include <QWheelEvent>

#include "Keyframe.h"

class KeyframeArea: public QWidget
{
Q_OBJECT

	int zoomFactor;
	QPoint rubberBandOrigin;
	QRubberBand* rubberBand;
	QPoint dragStartPosition;
	QFrame* dropIndicator;
	int dropIndex;
	QPoint rightMouseClickStartPostion;

    JointInfo::ListPtr m_jointConfig;

public:
	KeyframeArea(QWidget*);
	void addKeyframe(Keyframe*);
	void insertKeyframeAt(int, Keyframe*);
	void moveKeyframe(int, int);
	QPointer<Keyframe> getKeyframeByIndex(int);
	bool containsKeyframe(Keyframe*);
	QList< QPointer<Keyframe> > getKeyframes();
	bool isEmpty();
	void zoomIn();
	void zoomOut();
	void setZoom(int zoomFactor);
	void loadFile(QString filename);

signals:
	void keyframeDoubleClick(Keyframe*);
    void droppedFileName(QString);
    void jointConfigChanged(const JointInfo::ListPtr& config);

public slots:
	void clear();
	void clearSelection();
	void selectKeyframe(Keyframe*);
	void selectKeyframeByIndex(int);
	void deleteSelected();
	void interpolateSelected(double);

    void setJointConfig(const JointInfo::ListPtr& config);
protected:
	void dragEnterEvent(QDragEnterEvent*);
	void dragMoveEvent(QDragMoveEvent*);
	void dragLeaveEvent(QDragLeaveEvent*);
	void dropEvent(QDropEvent*);
	bool eventFilter(QObject*, QEvent*);
	void mouseMoveEvent(QMouseEvent*);
	void mousePressEvent(QMouseEvent*);
	void mouseReleaseEvent(QMouseEvent*);
	void mouseDoubleClickEvent(QMouseEvent*);
	void wheelEvent(QWheelEvent*);
	void keyPressEvent(QKeyEvent*);

private slots:
	void reindex();
};

#endif /* KEYFRAMEAREA_H_ */
