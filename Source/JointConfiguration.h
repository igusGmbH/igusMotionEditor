// Joint configuration manager
// Author: Max Schwarz <max.schwarz@uni-bonn.de>

#ifndef JOINTCONFIGURATION_H
#define JOINTCONFIGURATION_H

#include <QObject>
#include <QSettings>
#include <QSharedPointer>
#include <QVector>

struct JointInfo
{
    QString name;
    QString type;
    int address;

    // angle limits/offset in radians
    double upper_limit;
    double lower_limit;
    double offset;

    // encoder/motor to radians
    double enc_to_rad;
    double mot_to_rad;

    // nominal current setting
    double max_current;
    double hold_current;

    // axis length (distance to next joint) in m
    // may be negative if omitted.
    double length;

    // invert angles?
    bool invert;

    // Assigned joystick axis (negative if disabled)
    int joystick_axis;
    bool joystick_invert;

    class List : public QVector<JointInfo>
    {
    public:
        // Global options
        int lookahead;
    };
    typedef QSharedPointer<List> ListPtr;
};

class JointConfiguration : public QObject
{
    Q_OBJECT
    JointInfo::ListPtr m_config;
    QString m_error;

    void setError(const QString& arg);

public:
    explicit JointConfiguration(QObject *parent = 0);

    JointInfo::ListPtr config() const
    { return m_config; }

    const QString& error() const
    { return m_error; }
signals:
    void changed(const JointInfo::ListPtr& newConfig);
public slots:
    bool loadFromFile(const QString& filename);
    bool loadFromSettings(QSettings* settings);
};

#endif // JOINTCONFIGURATION_H
