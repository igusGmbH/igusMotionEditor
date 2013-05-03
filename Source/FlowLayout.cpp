/****************************************************************************
 **
 ** Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies).
 ** Contact: Qt Software Information (qt-info@nokia.com)
 **
 ** This file is part of the example classes of the Qt Toolkit.
 **
 ** Modified by Marcell for custom requirements of the KeyframeArea object.
 **
 **
 ****************************************************************************/

#include <QtGui>

#include "FlowLayout.h"

FlowLayout::FlowLayout(QWidget *parent, int spacing) :
	QLayout(parent)
{
	setSpacing(spacing);
}

FlowLayout::~FlowLayout()
{
	QLayoutItem *item;
	while ((item = takeAt(0)))
		delete item;
}

// Adds an item to the layout by appending it at the end.
void FlowLayout::addItem(QLayoutItem *item)
{
	itemList.append(item);
}

// Inserts a widget at the given index into the layout.
void FlowLayout::insertWidgetAt(int index, QWidget *item)
{
	// Insert the widget using the standard method.
	addWidget(item);

	//Now the widget is at the last position. Let's move it to the right place.
	itemList.insert(index, itemList.takeLast());
}

// Moves the widget.
void FlowLayout::moveWidget(int from, int to)
{
	if (from < 0)
		from = 0;
	if (from >= itemList.size())
		from = itemList.size()-1;
	if (to < 0)
		to = 0;
	if (to >= itemList.size())
		to = itemList.size()-1;

	itemList.move(from, to);
}

// Returns the number of items in the layout.
int FlowLayout::count() const
{
	return itemList.size();
}

// Returns the item saved at position index in the layout.
// The item is not removed from the layout.
QLayoutItem *FlowLayout::itemAt(int index) const
{
	return itemList.value(index);
}

// Returns the item saved at position index in the layout.
// The item is removed from the layout.
QLayoutItem *FlowLayout::takeAt(int index)
{
	if (index >= 0 && index < itemList.size())
		return itemList.takeAt(index);
	else
		return 0;
}

Qt::Orientations FlowLayout::expandingDirections() const
{
	return 0;
}

bool FlowLayout::hasHeightForWidth() const
{
	return true;
}

// Custom layout height calculation based on the flow layout principle.
int FlowLayout::heightForWidth(int width) const
{
	if (itemList.isEmpty())
		return 0;

	int itemWidth = itemList.at(0)->sizeHint().width();
	int itemHeight = itemList.at(0)->sizeHint().height();
	int itemCount = itemList.count();

	// How many items fit into the given width?
	int itemsPerRow = qMax(1, (width - 2*margin() + spacing()) / (itemWidth + spacing()));

	// How many rows will that be?
	int rows = (int)(qCeil((float)itemCount / itemsPerRow));

	// And that is how high?
	int height = 2 * margin() + rows * itemHeight + (rows - 1) * spacing();

	return height;
}

QSize FlowLayout::sizeHint() const
{
	return minimumSize();
}

void FlowLayout::setGeometry(const QRect &rect)
{
	if (itemList.isEmpty())
		return;

	QLayout::setGeometry(rect);

	int itemWidth = itemList.at(0)->sizeHint().width();
	int itemHeight = itemList.at(0)->sizeHint().height();

	int x = rect.x() + margin();
	int y = rect.y() + margin();

	QLayoutItem *item;
	foreach (item, itemList)
	{
		if ((x + itemWidth) > (rect.right() - margin()))
		{
			x = rect.x() + margin();
			y = y + itemHeight + spacing();
		}

		item->setGeometry(QRect(QPoint(x, y), QSize(itemWidth, itemHeight)));

		x = x + itemWidth + spacing();
	}

	emit rearranged();
}
