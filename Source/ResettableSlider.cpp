
#include "ResettableSlider.h"


ResettableSlider::ResettableSlider(QWidget *parent) :
	QSlider(parent)
{

}

void ResettableSlider::mouseDoubleClickEvent(QMouseEvent* event)
{
	setValue(0);
}

