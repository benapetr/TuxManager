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

#include "service.h"
#include "servicehelper.h"

#include <QRegularExpression>

using namespace OS;

namespace
{
    QList<Service> recordsToServices(const QList<ServiceHelper::ServiceRecord> &rows)
    {
        QList<Service> out;
        out.reserve(rows.size());
        for (const auto &r : rows)
        {
            Service s;
            s.Unit        = r.unit;
            s.Description = r.description;
            s.LoadState   = r.loadState;
            s.ActiveState = r.activeState;
            s.SubState    = r.subState;
            out.append(s);
        }
        return out;
    }
} // namespace

QList<Service> Service::LoadAll(QString *error)
{
    QList<Service> out;

    if (ServiceHelper::IsSystemdAvailable())
    {
        QList<ServiceHelper::ServiceRecord> rows;
        if (ServiceHelper::ListServicesViaSystemdDbus(rows, error))
        {
            if (error)
                error->clear();
            return recordsToServices(rows);
        }

        // Fallback for environments where sd-bus API isn't available but
        // systemctl is. This keeps the app functional on more minimal installs.
        QString stdout_text;
        QString stderr_text;
        int exit_code = -1;
        const QStringList args {
            "list-units",
            "--type=service",
            "--all",
            "--no-pager",
            "--no-legend",
            "--plain"
        };
        if (!ServiceHelper::RunSystemctl(args, stdout_text, stderr_text, exit_code) || exit_code != 0)
        {
            if (error)
                *error = stderr_text.isEmpty()
                         ? QObject::tr("Unable to query services via sd-bus or systemctl")
                         : stderr_text;
            return out;
        }

        static const QRegularExpression line_re(
            "^(\\S+)\\s+(\\S+)\\s+(\\S+)\\s+(\\S+)\\s*(.*)$");
        const QStringList lines = stdout_text.split('\n', Qt::SkipEmptyParts);
        for (const QString &raw_line : lines)
        {
            const QString line = raw_line.trimmed();
            if (line.isEmpty())
                continue;

            const QRegularExpressionMatch m = line_re.match(line);
            if (!m.hasMatch())
                continue;

            Service s;
            s.Unit        = m.captured(1);
            s.LoadState   = m.captured(2);
            s.ActiveState = m.captured(3);
            s.SubState    = m.captured(4);
            s.Description = m.captured(5).trimmed();
            out.append(s);
        }

        if (error)
            error->clear();
        return out;
    }

    if (ServiceHelper::IsRunitAvailable())
    {
        QList<ServiceHelper::ServiceRecord> rows;
        if (!ServiceHelper::ListServicesViaRunit(rows, error))
            return out;
        if (error)
            error->clear();
        return recordsToServices(rows);
    }

    if (error)
        *error = QObject::tr("No supported service manager found (systemd or runit)");
    return out;
}
