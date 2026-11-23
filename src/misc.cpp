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

#include "misc.h"
#include "historybuffer.h"

#include <array>
#include <QObject>
#include <QFile>
#include <QFileInfo>
#include <QtGlobal>

QString Misc::SimplifyTime(int secs)
{
    if (secs < 0)
        secs = 0;

    if (secs > 0 && (secs % 3600) == 0)
    {
        const int hours = secs / 3600;
        return QString::number(hours) + ((hours == 1) ? " hour" : " hours");
    }

    if (secs > 0 && (secs % 60) == 0)
    {
        const int minutes = secs / 60;
        return QString::number(minutes) + ((minutes == 1) ? " minute" : " minutes");
    }

    return QString::number(secs) + ((secs == 1) ? " second" : " seconds");
}

QString Misc::SimplifyTimeMS(int ms)
{
    if (ms < 0)
        ms = 0;

    if (ms > 0 && (ms % 1000) == 0)
        return QString::number(ms / 1000) + " s";

    return QString::number(ms) + " ms";
}

QString Misc::FormatBytesPerSecond(double bytesPerSec)
{
    const double v = (bytesPerSec < 0.0) ? 0.0 : bytesPerSec;

    if (v >= 1024.0 * 1024.0 * 1024.0)
        return QString::number(v / (1024.0 * 1024.0 * 1024.0), 'f', 2) + QObject::tr(" GB/s");

    if (v >= 1024.0 * 1024.0)
        return QString::number(v / (1024.0 * 1024.0), 'f', 1) + QObject::tr(" MB/s");

    if (v >= 1024.0)
        return QString::number(v / 1024.0, 'f', 0) + QObject::tr(" KB/s");

    return QString::number(v, 'f', 0) + QObject::tr(" B/s");
}

QString Misc::FormatBitsPerSecond(double bytesPerSec)
{
    const double v = (bytesPerSec < 0.0) ? 0.0 : (bytesPerSec * 8.0);

    if (v >= 1024.0 * 1024.0 * 1024.0)
        return QString::number(v / (1024.0 * 1024.0 * 1024.0), 'f', 2) + QObject::tr(" Gb/s");

    if (v >= 1024.0 * 1024.0)
        return QString::number(v / (1024.0 * 1024.0), 'f', 1) + QObject::tr(" Mb/s");

    if (v >= 1024.0)
        return QString::number(v / 1024.0, 'f', 0) + QObject::tr(" Kb/s");

    return QString::number(v, 'f', 0) + QObject::tr(" b/s");
}

QString Misc::FormatBytes(quint64 bytes, int precision)
{
    static const std::array<const char *, 5> kUnits = { " B", " KB", " MB", " GB", " TB" };

    double value = static_cast<double>(bytes);
    int unitIndex = 0;
    while (value >= 1024.0 && unitIndex < static_cast<int>(kUnits.size()) - 1)
    {
        value /= 1024.0;
        ++unitIndex;
    }

    const int decimals = (unitIndex == 0) ? 0 : qMax(0, precision);
    return QString::number(value, 'f', decimals) + QObject::tr(kUnits[unitIndex]);
}

QString Misc::FormatKiB(quint64 kibibytes, int precision)
{
    return FormatBytes(kibibytes * 1024ULL, precision);
}

QString Misc::FormatMiB(quint64 mebibytes, int precision)
{
    return FormatBytes(mebibytes * 1024ULL * 1024ULL, precision);
}

qint64 Misc::BytesToKiB(quint64 bytes)
{
    return static_cast<qint64>((bytes + 1023ULL) / 1024ULL);
}

QString Misc::ReadFile(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    const QString out = QString::fromUtf8(f.readAll()).trimmed();
    f.close();
    return out;
}

bool Misc::TextContainsAnyToken(const QString &text, const QStringList &tokens)
{
    const QString lower = text.toLower();
    for (const QString &t : tokens)
    {
        if (lower.contains(t))
            return true;
    }
    return false;
}

QString Misc::FileNameFromSymlink(const QString &path)
{
    const QString target = QFileInfo(path).symLinkTarget();
    if (target.isEmpty())
        return {};
    return QFileInfo(target).fileName();
}

void Misc::PushHistoryAndUpdateMax(HistoryBuffer &vec, double val, double &cachedMax, double minMax)
{
    double removed = 0.0;
    bool removedWasCurrentMax = false;
    if (vec.Size() >= vec.Capacity() && !vec.IsEmpty())
    {
        removed = vec.Front();
        removedWasCurrentMax = (removed >= cachedMax);
    }

    vec.Push(val);

    if (vec.IsEmpty())
    {
        cachedMax = minMax;
        return;
    }

    if (val >= cachedMax)
    {
        cachedMax = qMax(minMax, val);
        return;
    }

    if (removedWasCurrentMax)
    {
        double recomputedMax = minMax;
        for (int i = 0; i < vec.Size(); ++i)
            recomputedMax = qMax(recomputedMax, vec.At(i));
        cachedMax = recomputedMax;
        return;
    }

    cachedMax = qMax(cachedMax, minMax);
}
