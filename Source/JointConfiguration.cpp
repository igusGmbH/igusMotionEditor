#include "JointConfiguration.h"

#include <QRegExp>
#include <QStringList>
#include <QDebug>

#include <math.h>

JointConfiguration::JointConfiguration(QObject *parent) :
    QObject(parent)
{
}

bool JointConfiguration::loadFromFile(const QString& filename)
{
    QSettings settings(filename, QSettings::IniFormat);

    if(settings.status() != QSettings::NoError)
        return false;

    return loadFromSettings(&settings);
}

bool JointConfiguration::loadFromSettings(QSettings* settings)
{
    QRegExp joint_exp("^Joint(\\d+)$");

    // Should match the expression in Keyframe::validateString()
    QRegExp name_exp("^[\\w\\(\\)]+$");

    // Mandatory parameters
    QStringList mandatory;
    mandatory
            << "name" << "type" << "address" << "encoder_steps_per_turn" << "motor_steps_per_turn";

    JointInfo::List* list = new JointInfo::List;
    QVector<bool> addresses;

    // Init global options
    list->lookahead = settings->value("global/lookahead", 200).toUInt();

    QStringList groups = settings->childGroups();
    foreach(const QString& group, groups)
    {
        // Is this the global specification?
        if(group == "global")
            continue;

        // Is this a joint specification?
        if(!joint_exp.exactMatch(group))
        {
            setError(QString("Invalid group in configuration file: '%1'").arg(group));
            return false;
        }

        bool ok = true;
        int idx = joint_exp.cap(1).toInt(&ok);
        if(!ok)
        {
            setError(QString("Invalid group in configuration file: '%1'").arg(group));
            return false;
        }

        settings->beginGroup(group);

        // Check for must-have settings
        foreach(const QString& s, mandatory)
        {
            if(!settings->contains(s))
            {
                setError(QString("Group '%1' has no '%2' setting, which is mandatory").arg(group).arg(s));
                return false;
            }
        }

        JointInfo info;
        info.address = settings->value("address").toInt(&ok);
        if(!ok)
        {
            setError(QString("Invalid address setting in group '%1'").arg(group));
            return false;
        }

        if(addresses.size() >= info.address && addresses[info.address-1])
        {
            setError(QString("Address '%1' is used more than once").arg(info.address));
            return false;
        }

        info.name = settings->value("name").toString();

        if(!name_exp.exactMatch(info.name))
        {
            setError(QString(
                "The name '%1' contains invalid characters. "
                "Only alphanumeric characters and parentheses are allowed."
            ).arg(info.name));
            return false;
        }

        info.type = settings->value("type").toString();
        info.lower_limit = settings->value("lower_limit", -1.0).toDouble();
        info.upper_limit = settings->value("upper_limit", 1.0).toDouble();
        info.offset = settings->value("offset", 0.0).toDouble();
        info.length = settings->value("length", -1.0).toDouble();
        info.enc_to_rad = 2.0*M_PI / settings->value("encoder_steps_per_turn", 0.0).toDouble();
        info.mot_to_rad = 2.0*M_PI / settings->value("motor_steps_per_turn", 0.0).toDouble();
        info.joystick_axis = settings->value("joystick_axis", -1).toInt();
        info.joystick_invert = settings->value("joystick_invert", 0).toInt();
        info.invert = settings->value("invert", 0).toInt();

        settings->endGroup();

        if(idx >= list->size())
            list->resize(idx+1);
        (*list)[idx] = info;

        if(info.address > addresses.size())
            addresses.insert(addresses.size(), info.address - addresses.size(), false);
        addresses[info.address-1] = true;
    }

    // Sanity checks
    for(int i = 0; i < list->size(); ++i)
    {
        if(list->at(i).name.isEmpty())
        {
            setError(QString("Gap in joint specification at index '%1'. Make sure all joints are numbered correctly!").arg(i));
            return false;
        }
    }

    for(int i = 0; i < addresses.size(); ++i)
    {
        if(addresses[i] == false)
        {
            setError(QString("Address '%1' is not used. Addresses should be chosen without gaps.").arg(i+1));
            return false;
        }
    }

    qDebug() << "Joint configuration loaded.";
    m_config = JointInfo::ListPtr(list);
    emit changed(m_config);

    return true;
}

void JointConfiguration::setError(const QString& error)
{
    qDebug() << "Error:" << error;
    m_error = error;
}
