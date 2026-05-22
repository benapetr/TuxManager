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

#include "memory.h"
#include "../misc.h"
#include <QDir>
#include <QFile>

namespace
{
    struct ZramStats
    {
        qint64 CompressedKb { 0 };
        qint64 MemUsedKb { 0 };
        bool HasZram { false };
    };

    ZramStats readZramStats()
    {
        ZramStats stats;
        const QDir zramDir("/sys/block");
        const QStringList devices = zramDir.entryList(QStringList() << "zram*", QDir::Dirs | QDir::NoDotAndDotDot);

        for (const QString &device : devices)
        {
            const QString basePath = zramDir.absoluteFilePath(device);
            const QString diskSizeText = Misc::ReadFile(basePath + "/disksize");
            bool sizeOk = false;
            const quint64 diskSize = diskSizeText.toULongLong(&sizeOk);
            if (!sizeOk || diskSize == 0)
                continue;

            stats.HasZram = true;

            QFile mmStatFile(basePath + "/mm_stat");
            if (!mmStatFile.open(QIODevice::ReadOnly | QIODevice::Text))
                continue;

            const QStringList fields = QString::fromUtf8(mmStatFile.readLine()).simplified().split(' ', Qt::SkipEmptyParts);
            mmStatFile.close();
            if (fields.size() < 3)
                continue;

            bool origOk = false;
            bool memOk = false;
            const quint64 origBytes = fields.at(0).toULongLong(&origOk);
            const quint64 memBytes = fields.at(2).toULongLong(&memOk);
            if (origOk)
                stats.CompressedKb += Misc::BytesToKiB(origBytes);
            if (memOk)
                stats.MemUsedKb += Misc::BytesToKiB(memBytes);
        }

        return stats;
    }
}

Memory::Memory()
{
    this->readHardwareMetadata();
}

bool Memory::Sample()
{
    this->updateZswapEnabled();

    QFile f("/proc/meminfo");
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    qint64 memTotal = 0, memAvail = 0, memFree = 0;
    qint64 buffers  = 0, cached   = 0, sReclaimable = 0, shmem = 0;
    qint64 dirty    = 0, writeback = 0;
    qint64 zswapCompressed = 0, zswapPayload = 0;

    for (;;)
    {
        const QByteArray line = f.readLine();
        if (line.isNull())
            break;

        const int        colon = line.indexOf(':');
        if (colon < 0)
            continue;
        const QByteArray key = line.left(colon).trimmed();

        // /proc/meminfo values are formatted like "   12345 kB".
        // Extract the first token explicitly and validate conversion.
        const QByteArray valueField = line.mid(colon + 1).simplified();
        const QByteArray numberToken = valueField.split(' ').value(0);
        bool ok = false;
        const qint64 val = numberToken.toLongLong(&ok);
        if (!ok)
            continue;

        if      (key == "MemTotal")     memTotal     = val;
        else if (key == "MemFree")      memFree      = val;
        else if (key == "MemAvailable") memAvail     = val;
        else if (key == "Buffers")      buffers      = val;
        else if (key == "Cached")       cached       = val;
        else if (key == "SReclaimable") sReclaimable = val;
        else if (key == "Shmem")        shmem        = val;
        else if (key == "Dirty")        dirty        = val;
        else if (key == "Writeback")    writeback    = val;
        else if (this->m_zswapEnabled && key == "Zswap")    zswapCompressed = val;
        else if (this->m_zswapEnabled && key == "Zswapped") zswapPayload    = val;
    }
    f.close();

    // htop formula: used = total - free - buffers - page_cache
    // where page_cache = Cached + SReclaimable - Shmem
    const qint64 pageCache = cached + sReclaimable - shmem;
    const ZramStats zramStats = readZramStats();
    const qint64 memUsed = qMax(0LL, memTotal - memFree - buffers - pageCache);

    this->m_memTotalKb   = memTotal;
    this->m_memAvailKb   = memAvail;
    this->m_memFreeKb    = memFree;
    this->m_memBuffersKb = buffers;
    this->m_memDirtyKb   = dirty + writeback;
    this->m_zramCompressedKb = zramStats.CompressedKb;
    this->m_zramMemUsedKb = zramStats.MemUsedKb;
    this->m_hasZram = zramStats.HasZram;
    this->m_zswapPayloadKb = this->m_zswapEnabled ? zswapPayload : 0;
    this->m_zswapMemUsedKb = this->m_zswapEnabled ? zswapCompressed : 0;
    this->m_compressedPayloadKb = this->m_zramCompressedKb + this->m_zswapPayloadKb;
    this->m_compressedRamUsedKb = this->m_zramMemUsedKb + this->m_zswapMemUsedKb;
    this->m_hasCompressedMemory = this->m_hasZram || this->m_compressedPayloadKb > 0 || this->m_compressedRamUsedKb > 0;
    // Full page cache including buffers (what we show in stats and composition bar)
    this->m_memCachedKb  = buffers + pageCache;
    // RAM used, including compressed memory pools.
    this->m_memUsedKb    = memUsed;
    // Non-compressed in-use RAM for composition views that split compressed memory out separately.
    this->m_memUsedNonCompressedKb = qMax(0LL, memUsed - this->m_compressedRamUsedKb);

    // Graph tracks used / total (htop formula matches the green bar)
    const double frac = (memTotal > 0) ? static_cast<double>(this->m_memUsedKb) / static_cast<double>(memTotal) : 0.0;
    this->m_memHistory.Push(frac * 100.0);
    return true;
}

void Memory::updateZswapEnabled()
{
    if (this->m_zswapEnabledCheckTimer.isValid() && this->m_zswapEnabledCheckTimer.elapsed() < TUX_MANAGER_ZSWAP_ENABLED_CHECK_INTERVAL_MS)
        return;

    const QString enabledText = Misc::ReadFile("/sys/module/zswap/parameters/enabled").trimmed().toLower();
    this->m_zswapEnabled = enabledText == "y";

    if (!this->m_zswapEnabledCheckTimer.isValid())
        this->m_zswapEnabledCheckTimer.start();
    else
        this->m_zswapEnabledCheckTimer.restart();
}

void Memory::readHardwareMetadata()
{
    // Best-effort DIMM population and memory speed from SMBIOS type 17.
    const QDir entriesDir("/sys/firmware/dmi/entries");
    const QStringList type17 = entriesDir.entryList(QStringList() << "17-*", QDir::Dirs | QDir::NoDotAndDotDot);
    int slotsTotal = 0;
    int slotsUsed = 0;
    int speedMtps = 0;

    for (const QString &entry : type17)
    {
        QFile rawFile(entriesDir.absoluteFilePath(entry + "/raw"));
        if (!rawFile.open(QIODevice::ReadOnly))
            continue;

        const QByteArray raw = rawFile.readAll();
        rawFile.close();
        if (raw.size() < 0x17)
            continue;

        ++slotsTotal;

        const int structLen = static_cast<unsigned char>(raw.at(1));
        const quint16 sizeField = Misc::ReadLe16(raw, 12);

        qint64 sizeMb = 0;
        if (sizeField == 0 || sizeField == 0xFFFF)
            sizeMb = 0;
        else if (sizeField == 0x7FFF && structLen >= 0x20)
            sizeMb = static_cast<qint64>(Misc::ReadLe32(raw, 28));
        else if (sizeField & 0x8000)
            sizeMb = static_cast<qint64>(sizeField & 0x7FFF) / 1024; // value is in KiB
        else
            sizeMb = static_cast<qint64>(sizeField); // value is in MiB

        if (sizeMb > 0)
        {
            ++slotsUsed;
            const int speed = static_cast<int>(Misc::ReadLe16(raw, 21));
            int configured = 0;
            if (structLen >= 0x22)
                configured = static_cast<int>(Misc::ReadLe16(raw, 32));
            speedMtps = qMax(speedMtps, configured > 0 ? configured : speed);
        }
    }

    this->m_memDimmSlotsTotal = slotsTotal;
    this->m_memDimmSlotsUsed  = slotsUsed;
    this->m_memSpeedMtps      = speedMtps;
}

double Memory::MemFraction() const
{
    if (this->m_memTotalKb <= 0)
        return 0.0;
    return static_cast<double>(this->m_memUsedKb) / static_cast<double>(this->m_memTotalKb);
}
