#ifndef RESETABLESLIDER_H_
#define RESETABLESLIDER_H_

#include <QtGui>

class ResettableSlider: public QSlider
{
public:
	ResettableSlider(QWidget*);

protected:
	void mouseDoubleClickEvent(QMouseEvent*);

};

#endif /* RESETABLESLIDER_H_ */
