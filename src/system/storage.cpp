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

#include "storage.h"
#include "../misc.h"
#include <QDir>
#include <QFile>
#include <QtCore/qregularexpression.h>
#include <sys/statvfs.h>

Storage::Storage()
{

}

const Storage::DiskInfo &Storage::FromIndex(int i) const
{
    if (i < 0 || i >= static_cast<int>(this->m_disks.size()))
        return this->m_nullDisk;
    return *this->m_disks.at(i);
}

bool Storage::shouldIgnoreBlockDevice(const QString &baseName)
{
    return baseName.startsWith("loop")
        || baseName.startsWith("pktcdvd") // vmware packet optical drive
        || baseName.startsWith("sr")
        || baseName.startsWith("dm-")
        || baseName.startsWith("ram")
        || baseName.startsWith("zram");
}

QStringList Storage::listTrackedBlockDevices(const QSet<QString> &measurableDevices)
{
    QStringList out;
    const QDir sysBlockDir("/sys/block");
    const QStringList entries = sysBlockDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString &name : entries)
    {
        if (Storage::shouldIgnoreBlockDevice(name) || !measurableDevices.contains(name))
            continue;
        out.append(name);
    }
    return out;
}

QSet<QString> Storage::resolveBaseBlockDevices(const QString &devName)
{
    QSet<QString> out;
    if (devName.isEmpty() || devName == "." || devName == "..")
        return out;

    const QString sysPath = QString("/sys/class/block/%1").arg(devName);
    if (!QFileInfo::exists(sysPath))
        return out;

    // Device-mapper / md devices can have one or more "slaves".
    // If present, recurse into those and treat them as backing base devices.
    const QDir slavesDir(sysPath + "/slaves");
    const QStringList slaves = slavesDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    if (!slaves.isEmpty())
    {
        for (const QString &slave : slaves)
            out.unite(resolveBaseBlockDevices(slave));
        return out;
    }

    // Partitions expose /sys/class/block/<name>/partition.
    // Walk to parent block device (e.g. sda1 -> sda, nvme0n1p2 -> nvme0n1).
    if (QFileInfo::exists(sysPath + "/partition"))
    {
        QString parentName;
        const QString canonical = QFileInfo(sysPath).canonicalFilePath();
        if (!canonical.isEmpty())
        {
            QDir d(canonical);
            if (d.cdUp())
                parentName = d.dirName();
        }

        if ((parentName.isEmpty() || parentName == "." || parentName == "..") && !devName.isEmpty())
        {
            // Fallback when canonical parent lookup fails:
            // nvme0n1p2 -> nvme0n1, mmcblk0p1 -> mmcblk0, sda1 -> sda
            static const QRegularExpression partRe("^(.*?)(?:p)?\\d+$");
            const QRegularExpressionMatch m = partRe.match(devName);
            if (m.hasMatch())
                parentName = m.captured(1);
        }

        if (!parentName.isEmpty() && parentName != "." && parentName != ".." && parentName != devName)
            return resolveBaseBlockDevices(parentName);
    }

    out.insert(devName);
    return out;
}

void Storage::refreshDisks(const QSet<QString> &measurableDevices)
{
    QHash<QString, QSet<QString>> mountPointsByRaw;
    QHash<QString, qint64> formattedByRaw;
    QHash<QString, bool> rawHasSwap;

    // Mounted filesystems
    QFile mf("/proc/self/mountinfo");
    if (mf.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        for (;;)
        {
            const QByteArray line = mf.readLine();
            if (line.isNull())
                break;

            const QList<QByteArray> parts = line.trimmed().split(' ');
            const int sepIdx = parts.indexOf("-");
            if (sepIdx < 0 || sepIdx + 2 >= parts.size())
                continue;

            const QString source = QString::fromUtf8(parts.at(sepIdx + 2));
            if (!source.startsWith("/dev/"))
                continue;
            const QString raw = QFileInfo(source).fileName();

            const QString mountPoint = (parts.size() > 4) ? QString::fromUtf8(parts.at(4)) : QString();
            if (!mountPoint.isEmpty())
            {
                mountPointsByRaw[raw].insert(mountPoint);
                if (!formattedByRaw.contains(raw))
                {
                    struct statvfs vfs{};
                    if (::statvfs(mountPoint.toUtf8().constData(), &vfs) == 0)
                    {
                        formattedByRaw.insert(raw, static_cast<qint64>(vfs.f_blocks) * static_cast<qint64>(vfs.f_frsize));
                    }
                }
            }
        }
        mf.close();
    }

    // Swap devices are in use even when not mounted.
    QFile sf("/proc/swaps");
    if (sf.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        bool header = true;
        for (;;)
        {
            const QByteArray line = sf.readLine();
            if (line.isNull())
                break;
            if (header)
            {
                header = false;
                continue;
            }
            const QList<QByteArray> parts = line.simplified().split(' ');
            if (parts.isEmpty())
                continue;
            const QString src = QString::fromUtf8(parts.at(0));
            if (!src.startsWith("/dev/"))
                continue;
            const QString raw = QFileInfo(src).fileName();
            rawHasSwap.insert(raw, true);
        }
        sf.close();
    }

    const QStringList trackedDevices = listTrackedBlockDevices(measurableDevices);
    const QSet<QString> baseDevices(trackedDevices.cbegin(), trackedDevices.cend());
    QHash<QString, bool> systemByBase;
    QHash<QString, bool> pageFileByBase;
    QHash<QString, qint64> formattedByBase;

    for (auto it = mountPointsByRaw.cbegin(); it != mountPointsByRaw.cend(); ++it)
    {
        const QString &raw = it.key();
        const QSet<QString> bases = resolveBaseBlockDevices(raw);
        for (const QString &b : bases)
        {
            if (!baseDevices.contains(b))
                continue;

            const QSet<QString> &mountPoints = it.value();
            for (const QString &mp : mountPoints)
            {
                if (mp == "/")
                    systemByBase[b] = true;
            }
            formattedByBase[b] += formattedByRaw.value(raw, 0);
        }
    }

    for (auto it = rawHasSwap.cbegin(); it != rawHasSwap.cend(); ++it)
    {
        if (!it.value())
            continue;

        const QSet<QString> bases = resolveBaseBlockDevices(it.key());
        for (const QString &b : bases)
        {
            if (baseDevices.contains(b))
                pageFileByBase[b] = true;
        }
    }

    // Keep discovered disk objects persistent so their history vectors remain stable.
    QSet<QString> existingNames;
    for (const auto &disk : std::as_const(this->m_disks))
        existingNames.insert(disk->Name);

    for (const QString &name : trackedDevices)
    {
        if (existingNames.contains(name))
            continue;

        auto d = std::make_unique<DiskInfo>();
        d->Name = name;
        this->m_disks.push_back(std::move(d));
    }

    for (const auto &d : this->m_disks)
    {
        const QString &name = d->Name;
        if (name.isEmpty())
            continue;

        const QString model = Misc::ReadFile(QString("/sys/class/block/%1/device/model").arg(name));
        d->Model = model.isEmpty() ? QObject::tr("未知设备") : model;

        const QString rotational = Misc::ReadFile(QString("/sys/class/block/%1/queue/rotational").arg(name));
        if (rotational == "1")
            d->Type = "HDD";
        else if (rotational == "0")
            d->Type = "SSD";
        else
            d->Type = QObject::tr("未知");

        const qint64 sizeSecs = Misc::ReadFile(QString("/sys/class/block/%1/size").arg(name)).toLongLong();
        d->CapacityBytes = qMax<qint64>(0, sizeSecs) * 512LL;
        d->FormattedBytes = qMax<qint64>(0, formattedByBase.value(name, 0));
        d->IsSystemDisk = systemByBase.value(name, false);
        d->HasPageFile = pageFileByBase.value(name, false);
    }
}

bool Storage::Sample()
{
    QFile f("/proc/diskstats");
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    struct DiskCounters
    {
        quint64 readSectors  { 0 };
        quint64 writeSectors { 0 };
        quint64 ioMs         { 0 };
    };
    QHash<QString, DiskCounters> countersByName;
    QSet<QString> measurableDevices;

    for (;;)
    {
        const QByteArray line = f.readLine();
        if (line.isNull())
            break;
        const QList<QByteArray> parts = line.simplified().split(' ');
        if (parts.size() < 14)
            continue;

        DiskCounters c;
        c.readSectors  = parts.at(5).toULongLong();
        c.writeSectors = parts.at(9).toULongLong();
        c.ioMs         = parts.at(12).toULongLong();
        const QString devName = QString::fromUtf8(parts.at(2));
        countersByName.insert(devName, c);
        measurableDevices.insert(devName);
    }
    f.close();

    this->refreshDisks(measurableDevices);

    if (!this->m_diskTimer.isValid())
        this->m_diskTimer.start();

    const qint64 nowMs = this->m_diskTimer.elapsed();
    const qint64 dtMs = (this->m_prevDiskSampleMs > 0) ? (nowMs - this->m_prevDiskSampleMs) : 0;
    this->m_prevDiskSampleMs = nowMs;

    static constexpr double kSectorBytes = 512.0;

    for (const auto &d : this->m_disks)
    {
        const auto it = countersByName.constFind(d->Name);
        if (it == countersByName.cend())
        {
            d->ActivePct = 0.0;
            d->ReadBps = 0.0;
            d->WriteBps = 0.0;
            d->ActiveHistory.Push(0.0);
            Misc::PushHistoryAndUpdateMax(d->ReadHistory, 0.0, d->MaxTransferBps);
            Misc::PushHistoryAndUpdateMax(d->WriteHistory, 0.0, d->MaxTransferBps);
            continue;
        }

        const DiskCounters c = it.value();
        if (dtMs <= 0)
        {
            d->PrevReadSecs  = c.readSectors;
            d->PrevWriteSecs = c.writeSectors;
            d->PrevIoMs      = c.ioMs;
            d->ActiveHistory.Push(0.0);
            Misc::PushHistoryAndUpdateMax(d->ReadHistory, 0.0, d->MaxTransferBps);
            Misc::PushHistoryAndUpdateMax(d->WriteHistory, 0.0, d->MaxTransferBps);
            continue;
        }

        const quint64 dReadSecs  = (c.readSectors  >= d->PrevReadSecs ) ? (c.readSectors  - d->PrevReadSecs ) : 0;
        const quint64 dWriteSecs = (c.writeSectors >= d->PrevWriteSecs) ? (c.writeSectors - d->PrevWriteSecs) : 0;
        const quint64 dIoMs      = (c.ioMs         >= d->PrevIoMs     ) ? (c.ioMs         - d->PrevIoMs     ) : 0;

        d->PrevReadSecs  = c.readSectors;
        d->PrevWriteSecs = c.writeSectors;
        d->PrevIoMs      = c.ioMs;

        d->ActivePct = qBound(0.0, static_cast<double>(dIoMs) * 100.0 / static_cast<double>(dtMs), 100.0);
        d->ReadBps  = static_cast<double>(dReadSecs)  * kSectorBytes * 1000.0 / static_cast<double>(dtMs);
        d->WriteBps = static_cast<double>(dWriteSecs) * kSectorBytes * 1000.0 / static_cast<double>(dtMs);

        d->ActiveHistory.Push(d->ActivePct);
        Misc::PushHistoryAndUpdateMax(d->ReadHistory, d->ReadBps, d->MaxTransferBps);
        Misc::PushHistoryAndUpdateMax(d->WriteHistory, d->WriteBps, d->MaxTransferBps);
    }

    return true;
}
