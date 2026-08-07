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

QList<Service> Service::LoadAll(QString *error)
{
    QList<Service> out;

    QString reason;
    if (!ServiceHelper::IsSystemdAvailable(&reason))
    {
        if (error)
            *error = reason;
        return out;
    }

    QList<ServiceHelper::ServiceRecord> rows;
    if (ServiceHelper::ListServicesViaSystemdDbus(rows, error))
    {
        out.reserve(rows.size());
        for (const auto &r : rows)
        {
            Service s;
            s.Unit = r.unit;
            s.Description = r.description;
            s.LoadState = r.loadState;
            s.ActiveState = r.activeState;
            s.SubState = r.subState;
            out.append(s);
        }
        if (error)
            error->clear();
        return out;
    }

    // Fallback for environments where sd-bus API isn't available but systemctl
    // is. This still keeps the app functional on more minimal installs.
    QString stdoutText;
    QString stderrText;
    int exitCode = -1;
    const QStringList args {
        "list-units",
        "--type=service",
        "--all",
        "--no-pager",
        "--no-legend",
        "--plain"
    };
    if (!ServiceHelper::RunSystemctl(args, stdoutText, stderrText, exitCode, kSystemctlQueryTimeoutMs) || exitCode != 0)
    {
        if (error)
            *error = stderrText.isEmpty()
                     ? QObject::tr("无法通过 sd-bus 或 systemctl 查询服务")
                     : stderrText;
        return out;
    }

    const QRegularExpression lineRe("^(\\S+)\\s+(\\S+)\\s+(\\S+)\\s+(\\S+)\\s*(.*)$");
    const QStringList lines = stdoutText.split('\n', Qt::SkipEmptyParts);
    for (const QString &rawLine : lines)
    {
        const QString line = rawLine.trimmed();
        if (line.isEmpty())
            continue;

        const QRegularExpressionMatch m = lineRe.match(line);
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
