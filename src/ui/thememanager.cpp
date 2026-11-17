/*
 * Tux Manager - Linux system monitor
 * Copyright (C) 2026 Petr Bena <petr@bena.rocks>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "thememanager.h"
#include "../colorscheme.h"
#include "../configuration.h"

#include <QApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QScopedValueRollback>
#include <QStyle>
#include <QStyleFactory>
#include <QTimer>
#include <memory>

namespace
{
    const QString portalService = QStringLiteral("org.freedesktop.portal.Desktop");
    const QString portalPath = QStringLiteral("/org/freedesktop/portal/desktop");
    const QString portalInterface = QStringLiteral("org.freedesktop.portal.Settings");

    QVariant unwrap(QVariant value)
    {
        // Older portal Read replies wrap the value in two variants.
        while (value.userType() == qMetaTypeId<QDBusVariant>())
            value = value.value<QDBusVariant>().variant();
        return value;
    }

    QVariant readPreference(const QString &nameSpace)
    {
        for (const QString &method : { QStringLiteral("ReadOne"), QStringLiteral("Read") })
        {
            QDBusMessage request = QDBusMessage::createMethodCall(portalService, portalPath, portalInterface, method);
            request.setArguments({ nameSpace, QStringLiteral("color-scheme") });
            const QDBusMessage reply = QDBusConnection::sessionBus().call(request, QDBus::Block, 500);
            if (reply.type() == QDBusMessage::ReplyMessage && !reply.arguments().isEmpty())
                return unwrap(reply.arguments().first());
        }
        return QVariant();
    }

    QPalette appearancePalette(bool dark)
    {
        std::unique_ptr<QStyle> fusion(QStyleFactory::create("Fusion"));
        QPalette palette = fusion ? fusion->standardPalette() : QPalette();
        if (!dark)
            return palette;

        palette.setColor(QPalette::Window, QColor("#292b30"));
        palette.setColor(QPalette::WindowText, QColor("#eeeeee"));
        palette.setColor(QPalette::Base, QColor("#202226"));
        palette.setColor(QPalette::AlternateBase, QColor("#303238"));
        palette.setColor(QPalette::Text, QColor("#eeeeee"));
        palette.setColor(QPalette::Button, QColor("#35373d"));
        palette.setColor(QPalette::ButtonText, QColor("#eeeeee"));
        palette.setColor(QPalette::BrightText, Qt::white);
        palette.setColor(QPalette::ToolTipBase, QColor("#35373d"));
        palette.setColor(QPalette::ToolTipText, QColor("#eeeeee"));
        palette.setColor(QPalette::Light, QColor("#55585f"));
        palette.setColor(QPalette::Midlight, QColor("#44474d"));
        palette.setColor(QPalette::Mid, QColor("#222429"));
        palette.setColor(QPalette::Dark, QColor("#191b1e"));
        palette.setColor(QPalette::Shadow, Qt::black);
        palette.setColor(QPalette::Highlight, QColor("#3478c5"));
        palette.setColor(QPalette::HighlightedText, Qt::white);
        palette.setColor(QPalette::Link, QColor("#70b7ff"));
        palette.setColor(QPalette::LinkVisited, QColor("#c59bff"));
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette.setColor(QPalette::PlaceholderText, QColor("#aaaaaa"));
#endif
        for (QPalette::ColorRole role : { QPalette::WindowText, QPalette::Text, QPalette::ButtonText })
            palette.setColor(QPalette::Disabled, role, QColor("#909399"));
        palette.setColor(QPalette::Disabled, QPalette::Highlight, QColor("#454851"));
        palette.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor("#b5b7bc"));
        return palette;
    }
}

ThemeManager *ThemeManager::Get()
{
    static ThemeManager *instance = new ThemeManager(qApp);
    return instance;
}

ThemeManager::ThemeManager(QObject *parent) : QObject(parent)
    , m_systemPalette(QApplication::palette())
    , m_systemStyle(QApplication::style()->objectName())
{
    QDBusConnection::sessionBus().connect(portalService, portalPath, portalInterface, "SettingChanged",
        this, SLOT(onSettingChanged(QString,QString,QDBusVariant)));
    this->m_portalPreference = readPreference("org.freedesktop.appearance").toInt();
    // Some GNOME/Cinnamon portal versions report no standard preference while
    // exposing the desktop's actual preference in this namespace.
    this->m_desktopPreference = readPreference("org.gnome.desktop.interface").toString();

    connect(qApp, &QGuiApplication::paletteChanged, this, [this](const QPalette &palette)
    {
        if (this->m_applying)
            return;
        this->m_systemPalette = palette;
        QTimer::singleShot(0, this, &ThemeManager::Apply);
    });
}

bool ThemeManager::systemDark() const
{
    if (this->m_portalPreference == 1 || this->m_portalPreference == 2)
        return this->m_portalPreference == 1;
    if (this->m_desktopPreference == "prefer-dark" || this->m_desktopPreference == "prefer-light")
        return this->m_desktopPreference == "prefer-dark";
    return this->m_systemPalette.color(QPalette::Window).lightness() <= 127;
}

void ThemeManager::onSettingChanged(const QString &nameSpace, const QString &key, const QDBusVariant &value)
{
    if (key != "color-scheme")
        return;
    if (nameSpace == "org.freedesktop.appearance")
        this->m_portalPreference = unwrap(value.variant()).toInt();
    else if (nameSpace == "org.gnome.desktop.interface")
        this->m_desktopPreference = unwrap(value.variant()).toString();
    else
        return;

    if (CFG->AppearanceMode == "system")
        this->Apply();
}

void ThemeManager::SetMode(const QString &mode)
{
    CFG->AppearanceMode = (mode == "light" || mode == "dark") ? mode : QStringLiteral("system");
    this->Apply();
    CFG->Save();
}

void ThemeManager::Apply()
{
    if (this->m_applying)
        return;
    QScopedValueRollback<bool> guard(this->m_applying, true);
    const bool dark = CFG->AppearanceMode == "dark" || (CFG->AppearanceMode == "system" && this->systemDark());
    const bool nativeDark = this->m_systemPalette.color(QPalette::Window).lightness() <= 127;
    const bool useNative = CFG->AppearanceMode == "system" && dark == nativeDark;
    const QString style = useNative ? this->m_systemStyle : QStringLiteral("fusion");
    if (QApplication::style()->objectName().compare(style, Qt::CaseInsensitive) != 0)
        QApplication::setStyle(style);
    QApplication::setPalette(useNative ? this->m_systemPalette : appearancePalette(dark));

    auto *scheme = new ColorScheme(dark ? ColorScheme::DefaultDark() : ColorScheme::DefaultLight());
    if (CFG->UseCustomColorScheme)
        scheme->ApplyVariantMap(CFG->CustomColorScheme);
    scheme->DarkMode = dark;
    ColorScheme::Install(scheme);
    emit this->Changed();
}
