#include "flashtool.h"
#include "ui_flashtool.h"

#define _WIN32_WINDOWS 0x0410
#define WINVER 0x0500

#include "../Serial.h"
#include "../microcontroller/protocol.h"



#include <Windows.h>
#include <winuser.h>
#include <dbt.h>
#include <basetyps.h>
#include <initguid.h>
#include <ddk/ntddser.h>

#include <QtCore/QDebug>
#include <QtCore/QProcess>
#include <QtGui/QMessageBox>


FlashTool::FlashTool(QWidget *parent)
 : QWidget(parent)
 , ui(new Ui::FlashTool)
{
    ui->setupUi(this);

    updatePorts();

    // Listen to device events
    DEV_BROADCAST_DEVICEINTERFACE devInt;
    ZeroMemory(&devInt, sizeof(devInt));
    devInt.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
    devInt.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    devInt.dbcc_classguid = GUID_DEVINTERFACE_COMPORT;

    HANDLE blub;
    blub = RegisterDeviceNotification(winId(), &devInt, DEVICE_NOTIFY_WINDOW_HANDLE);


    connect(ui->bootFlashButton, SIGNAL(clicked()), SLOT(flashBootloader()));
    connect(ui->flashButton, SIGNAL(clicked()), SLOT(flashFirmware()));
}

FlashTool::~FlashTool()
{
    delete ui;
}

void FlashTool::updatePorts()
{
    CSerial serial;

    ui->progPortBox->clear();
    ui->ucPortBox->clear();

    for(int i = 1; i < 50; ++i)
    {
        QString path = "\\\\.\\COM" + QString::number(i);
        if(serial.Open(path))
        {
            ui->progPortBox->addItem("COM" + QString::number(i), path);
            ui->ucPortBox->addItem("COM" + QString::number(i), path);
            serial.close();
        }
    }
}

bool FlashTool::winEvent(MSG *message, long *result)
{
    //qDebug() << message;
    if(message->message == WM_DEVICECHANGE)
        updatePorts();
    return false;
}

void FlashTool::flashBootloader()
{
    QString prog = "cmd.exe";
    QStringList args;
    args
         << "/K" << "microcontroller\\avrdude.exe"
         << "-c" << ui->progTypeEdit->text()
         << "-p" << "atmega2560"
         << "-P" << ui->progPortBox->itemData(ui->progPortBox->currentIndex()).toString()
         << "-U" << "lfuse:w:0xCE:m"
         << "-U" << "hfuse:w:0xD4:m"
         << "-U" << "efuse:w:0xFC:m"
         << "-U" << "flash:w:microcontroller\\bootloader.hex";

    QProcess::startDetached(prog, args);
    return;
}

void FlashTool::enterBootloader()
{
    CSerial serial;
    if(!serial.Open(ui->ucPortBox->itemData(ui->ucPortBox->currentIndex()).toString()))
    {
        QMessageBox::critical(this, "Error", "Could not open serial port.");
        return;
    }

    serial.Setup(CSerial::EBaud115200);
    serial.SetupHandshaking(CSerial::EHandshakeOff);
    serial.SetEventChar(0x0D);

    int version;

    proto::SimplePacket<proto::CMD_INIT> init;
    for(version = 0; version <= proto::VERSION+10; ++version)
    {
        init.header.version = version;
        init.checksum = proto::packetChecksum(init.header, 0);

        serial.write(&init, sizeof(init));
        if(serial.WaitEvent(200) == ERROR_SUCCESS)
            break;
    }

    proto::Packet<proto::CMD_RESET, proto::Reset> reset;
    reset.header.version = version;
    memcpy(&reset.payload.key, proto::RESET_KEY, sizeof(proto::RESET_KEY));
    reset.updateChecksum();

    serial.write(&reset, sizeof(reset));
}

void FlashTool::flashFirmware()
{
    enterBootloader();

    QString prog = "cmd.exe";
    QStringList args;
    args
         << "/K" << "microcontroller\\avrdude.exe"
         << "-c" << "avr109"
         << "-p" << "atmega2560"
         << "-b" << "115200"
         << "-P" << ui->ucPortBox->itemData(ui->ucPortBox->currentIndex()).toString()
         << "-U" << "flash:w:microcontroller\\microcontroller.hex";

    QProcess::startDetached(prog, args);
    return;
}
