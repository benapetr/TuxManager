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

#ifndef UI_THEMEMANAGER_H
#define UI_THEMEMANAGER_H

#include <QObject>
#include <QPalette>
#include <QDBusVariant>

// Applies the application appearance independently of Qt's desktop integration.
// The portal supplies live system preferences on both Qt 5 and Qt 6.
class ThemeManager : public QObject
{
    Q_OBJECT

    public:
        static ThemeManager *Get();
        void Apply();
        void SetMode(const QString &mode);

    signals:
        void Changed();

    private slots:
        void onSettingChanged(const QString &nameSpace, const QString &key, const QDBusVariant &value);

    private:
        explicit ThemeManager(QObject *parent);
        bool systemDark() const;

        QPalette m_systemPalette;
        QString m_systemStyle;
        int m_portalPreference { 0 };
        QString m_desktopPreference;
        bool m_applying { false };
};

#endif
