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

#ifndef OS_SERVICEHELPER_H
#define OS_SERVICEHELPER_H

#include <QList>
#include <QString>
#include <QStringList>

namespace OS
{
    inline constexpr int kSystemctlQueryTimeoutMs = 8000;
    inline constexpr int kSystemctlManageTimeoutMs = 120000;

    class ServiceHelper
    {
        public:
            enum class UnitAction
            {
                Start,
                Stop,
                Restart,
                Reload,
                TryRestart,
                ReloadOrRestart
            };

            struct ServiceRecord
            {
                QString unit;
                QString description;
                QString loadState;
                QString activeState;
                QString subState;
            };

            static bool IsSystemdAvailable(QString *reason = nullptr);
            static bool RunSystemctl(const QStringList &args,
                                     QString          &stdoutText,
                                     QString          &stderrText,
                                     int              &exitCode,
                                     int               timeoutMs = 8000);
            static bool ListServicesViaSystemdDbus(QList<ServiceRecord> &records,
                                                   QString              *error = nullptr);
            static bool ManageUnit(const QString &unit,
                                   UnitAction     action,
                                   QString       *error = nullptr);

            static bool IsRunitAvailable(QString *reason = nullptr);
            static bool RunSv(const QStringList &args,
                              QString          &stdoutText,
                              QString          &stderrText,
                              int              &exitCode,
                              int               timeoutMs = 10000);
            static bool ListServicesViaRunit(QList<ServiceRecord> &records,
                                             QString              *error = nullptr);

        private:
            ServiceHelper() = delete;
    };
} // namespace Os

#endif // OS_SERVICEHELPER_H
