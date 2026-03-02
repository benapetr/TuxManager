#include "configuration.h"

#include <QSettings>

Configuration *Configuration::s_instance = nullptr;

Configuration *Configuration::instance()
{
    if (!s_instance)
        s_instance = new Configuration();
    return s_instance;
}

Configuration::Configuration(QObject *parent) : QObject(parent)
{}

void Configuration::Load()
{
    QSettings s;

    // Window
    this->WindowGeometry = s.value("Window/Geometry").toByteArray();
    this->WindowState    = s.value("Window/State").toByteArray();
    this->ActiveTab      = s.value("Window/ActiveTab", this->ActiveTab).toInt();

    // General
    this->RefreshRateMs  = s.value("General/RefreshRateMs", this->RefreshRateMs).toInt();
}

void Configuration::Save()
{
    QSettings s;

    // Window
    s.setValue("Window/Geometry",  this->WindowGeometry);
    s.setValue("Window/State",     this->WindowState);
    s.setValue("Window/ActiveTab", this->ActiveTab);

    // General
    s.setValue("General/RefreshRateMs", this->RefreshRateMs);

    s.sync();
}
