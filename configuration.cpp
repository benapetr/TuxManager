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
    WindowGeometry = s.value("Window/Geometry").toByteArray();
    WindowState    = s.value("Window/State").toByteArray();
    ActiveTab      = s.value("Window/ActiveTab", ActiveTab).toInt();

    // General
    RefreshRateMs  = s.value("General/RefreshRateMs", RefreshRateMs).toInt();
}

void Configuration::Save()
{
    QSettings s;

    // Window
    s.setValue("Window/Geometry",  WindowGeometry);
    s.setValue("Window/State",     WindowState);
    s.setValue("Window/ActiveTab", ActiveTab);

    // General
    s.setValue("General/RefreshRateMs", RefreshRateMs);

    s.sync();
}
