#ifndef FLASHTOOL_H
#define FLASHTOOL_H

#include <QWidget>

namespace Ui {
class FlashTool;
}

class FlashTool : public QWidget
{
    Q_OBJECT
    
public:
    explicit FlashTool(QWidget *parent = 0);
    ~FlashTool();

    virtual bool winEvent(MSG *message, long *result);
private slots:
    void updatePorts();
    void flashBootloader();
    void enterBootloader();
    void flashFirmware();
private:
    Ui::FlashTool *ui;
};

#endif // FLASHTOOL_H
