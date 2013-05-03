#include <QtGui/QApplication>
#include "flashtool.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    FlashTool w;
    w.show();
    
    return a.exec();
}
