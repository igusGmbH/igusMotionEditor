#include <QtGui>
#include <QApplication>
#include <QtDebug>
#include "IgusMotionEditor.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // Register custom types
    qRegisterMetaType<JointInfo::ListPtr>("JointInfo::ListPtr");

	// Apply a stylesheet to the application.
	QFile file("styles.css");
	file.open(QFile::ReadOnly);
	QString styleSheet = QLatin1String(file.readAll());
	a.setStyleSheet(styleSheet);
	//a.setStyle("plastique");
	//a.setStyle("clearlooks");

	IgusMotionEditor w;
	w.showMaximized();

	return a.exec();
}
