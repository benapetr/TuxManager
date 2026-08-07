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

#include "drmbackend.h"
#include "../../logger.h"
#include "../../misc.h"

#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <QSysInfo>
#include <limits.h>
#include <unistd.h>

namespace
{
    bool isIntelDrmDriver(const QString &driverName)
    {
        return driverName == QLatin1String("i915") || driverName == QLatin1String("xe");
    }

    bool isAmdDrmDriver(const QString &driverName)
    {
        return driverName == QLatin1String("amdgpu");
    }

    bool shouldIgnoreDrmGpu(const QString &driverName, const QString &uevent)
    {
        static const QStringList ignoreTokens {
            QStringLiteral("xrdpdev"),
            QStringLiteral("xrdp"),
            QStringLiteral("vmwgfx"),
            QStringLiteral("vmware"),
            QStringLiteral("vkms"),
            QStringLiteral("vgem"),
            QStringLiteral("virtio_gpu"),
            QStringLiteral("virtio"),
            QStringLiteral("hyperv_drm"),
            QStringLiteral("simpledrm"),
            QStringLiteral("bochs-drm"),
            QStringLiteral("bochs_drm"),
            QStringLiteral("bochs"),
            QStringLiteral("qxl")
        };

        return Misc::TextContainsAnyToken(driverName, ignoreTokens) || Misc::TextContainsAnyToken(uevent, ignoreTokens);
    }

    GPU::GPUInfo *findOrCreateGpu(std::vector<std::unique_ptr<GPU::GPUInfo>> &gpus, const QString &id)
    {
        for (const auto &gpu : gpus)
        {
            if (gpu->ID == id)
                return gpu.get();
        }

        auto gpu = std::make_unique<GPU::GPUInfo>();
        gpu->ID = id;
        gpus.push_back(std::move(gpu));
        return gpus.back().get();
    }

    void zeroMissingEngines(GPU::GPUInfo &gpu, const QSet<QString> &seenEngineKeys)
    {
        for (const auto &engine : gpu.Engines)
        {
            if (seenEngineKeys.contains(engine->Key))
                continue;

            engine->Pct = 0.0;
            engine->History.Push(0.0);
        }
    }
}

void GpuDrmBackend::Detect(bool skipNvidia, bool skipAmd, bool skipIntel)
{
    LOG_DEBUG("Detecting DRM GPUs");

    this->m_cards.clear();

    const QDir drmDir(QStringLiteral("/sys/class/drm"));
    const QStringList entries = drmDir.entryList({QStringLiteral("card[0-9]*")}, QDir::Dirs);

    for (const QString &entry : entries)
    {
        if (entry.contains(QLatin1Char('-')))
            continue;

        const QString devPath = drmDir.filePath(entry + QStringLiteral("/device"));
        const QString vendorStr = Misc::ReadFile(devPath + QStringLiteral("/vendor")).trimmed();
        const QString uevent = Misc::ReadFile(devPath + QStringLiteral("/uevent"));

        DRMCard card;
        card.Vendor = vendorStr;
        card.CardNodePath = QStringLiteral("/dev/dri/") + entry;

        const QString canonical = QFileInfo(devPath).canonicalFilePath();
        card.ID = QFileInfo(canonical).fileName();
        if (card.ID.isEmpty())
            card.ID = entry;

        const QStringList renderNodes = QDir(devPath + QStringLiteral("/drm")).entryList({QStringLiteral("renderD*")}, QDir::Dirs);
        if (!renderNodes.isEmpty())
            card.RenderNodePath = QStringLiteral("/dev/dri/") + renderNodes.first();

        const QStringList hwmons = QDir(devPath + QStringLiteral("/hwmon")).entryList({QStringLiteral("hwmon[0-9]*")}, QDir::Dirs);
        if (!hwmons.isEmpty())
        {
            const QString hwPath = devPath + QStringLiteral("/hwmon/") + hwmons.first();
            card.DriverName = Misc::ReadFile(hwPath + QStringLiteral("/name")).trimmed();
            const QString tempPath = hwPath + QStringLiteral("/temp1_input");
            if (QFileInfo::exists(tempPath))
                card.TempPath = tempPath;
        }

        if (card.DriverName.isEmpty())
            card.DriverName = Misc::FileNameFromSymlink(devPath + QStringLiteral("/driver/module"));
        if (card.DriverName.isEmpty())
            card.DriverName = Misc::FileNameFromSymlink(devPath + QStringLiteral("/driver"));

        if (shouldIgnoreDrmGpu(card.DriverName, uevent))
            continue;

        if ((skipNvidia && vendorStr == QLatin1String("0x10de"))
            || (skipAmd && vendorStr == QLatin1String("0x1002"))
            || (skipIntel && vendorStr == QLatin1String("0x8086")))
            continue;

        if (!card.DriverName.isEmpty())
        {
            const QString version = Misc::ReadFile(QStringLiteral("/sys/module/") + card.DriverName + QStringLiteral("/version")).trimmed();
            if (!version.isEmpty())
                card.DriverVersion = version;
        }
        if (card.DriverVersion.isEmpty())
            card.DriverVersion = QSysInfo::kernelVersion();

        const QString busyPath = devPath + QStringLiteral("/gpu_busy_percent");
        if (QFileInfo::exists(busyPath))
            card.BusyPath = busyPath;

        const QStringList busyFiles = QDir(devPath).entryList({QStringLiteral("*_busy_percent")}, QDir::Files);
        for (const QString &file : busyFiles)
        {
            if (file == QLatin1String("gpu_busy_percent"))
                continue;

            static const QLatin1String suffix("_busy_percent");
            const QString key = file.chopped(suffix.size());
            card.EngineBusyPaths.append({key, devPath + QLatin1Char('/') + file});
        }

        const QString vramTotal = devPath + QStringLiteral("/mem_info_vram_total");
        const QString vramUsed = devPath + QStringLiteral("/mem_info_vram_used");
        if (QFileInfo::exists(vramTotal) && QFileInfo::exists(vramUsed))
        {
            card.VramTotalPath = vramTotal;
            card.VramUsedPath = vramUsed;
        }

        const QString gttTotal = devPath + QStringLiteral("/mem_info_gtt_total");
        const QString gttUsed = devPath + QStringLiteral("/mem_info_gtt_used");
        if (QFileInfo::exists(gttTotal) && QFileInfo::exists(gttUsed))
        {
            card.GttTotalPath = gttTotal;
            card.GttUsedPath = gttUsed;
        }

        if (isAmdDrmDriver(card.DriverName) || card.Vendor == QLatin1String("0x1002"))
            card.Sampler = SamplerKind::Amd;
        else if (isIntelDrmDriver(card.DriverName) || card.Vendor == QLatin1String("0x8086"))
            card.Sampler = SamplerKind::Intel;
        else
        {
            const QString driverLabel = card.DriverName.isEmpty() ? QStringLiteral("<unknown>") : card.DriverName;
            LOG_DEBUG(QStringLiteral("Ignoring unsupported DRM GPU %1 (vendor=%2, driver=%3)")
                          .arg(entry, vendorStr, driverLabel));
            continue;
        }

        LOG_DEBUG("Found DRM GPU: " + entry);
        this->m_cards.append(card);
    }
}

bool GpuDrmBackend::Sample(std::vector<std::unique_ptr<GPU::GPUInfo>> &gpus)
{
    const qint64 fdInfoElapsedNs = this->m_fdInfoTimerStarted ? this->m_fdInfoTimer.nsecsElapsed() : 0;

    ++this->m_fdInfoRescanCounter;

    for (DRMCard &card : this->m_cards)
    {
        GPU::GPUInfo &gpu = *findOrCreateGpu(gpus, card.ID);
        gpu.ID = card.ID;
        gpu.DriverVersion = card.DriverVersion;
        gpu.Backend = card.DriverName.isEmpty() ? QStringLiteral("drm") : card.DriverName;

        if (card.Vendor == QLatin1String("0x1002"))
            gpu.Name = QStringLiteral("AMD Radeon");
        else if (card.Vendor == QLatin1String("0x8086"))
            gpu.Name = QStringLiteral("Intel Graphics");
        else
            gpu.Name = QStringLiteral("GPU");

        switch (card.Sampler)
        {
            case SamplerKind::Amd:
                this->sampleAmdCard(card, gpu, fdInfoElapsedNs);
                break;
            case SamplerKind::Intel:
                this->sampleIntelCard(card, gpu, fdInfoElapsedNs);
                break;
        }
    }

    this->m_fdInfoTimer.start();
    this->m_fdInfoTimerStarted = true;

    return !this->m_cards.isEmpty();
}

bool GpuDrmBackend::sampleAmdCard(DRMCard &card, GPU::GPUInfo &gpu, qint64 fdInfoElapsedNs)
{
    gpu.TemperatureC = -1;
    gpu.CoreClockMHz = -1;
    gpu.PowerUsageW = -1.0;

    if (!card.BusyPath.isEmpty())
    {
        bool ok = false;
        const int pct = Misc::ReadFile(card.BusyPath).trimmed().toInt(&ok);
        gpu.UtilPct = ok ? qBound(0.0, static_cast<double>(pct), 100.0) : 0.0;
    }
    else
    {
        gpu.UtilPct = 0.0;
    }
    gpu.UtilHistory.Push(gpu.UtilPct);

    if (!card.TempPath.isEmpty())
    {
        bool ok = false;
        const int milliC = Misc::ReadFile(card.TempPath).trimmed().toInt(&ok);
        gpu.TemperatureC = ok ? milliC / 1000 : -1;
    }

    if (!card.VramTotalPath.isEmpty())
    {
        bool okTotal = false;
        bool okUsed = false;
        const qint64 total = Misc::ReadFile(card.VramTotalPath).trimmed().toLongLong(&okTotal);
        const qint64 used = Misc::ReadFile(card.VramUsedPath).trimmed().toLongLong(&okUsed);
        gpu.MemTotalMiB = okTotal ? total / (1024LL * 1024LL) : 0;
        gpu.MemUsedMiB = okUsed ? used / (1024LL * 1024LL) : 0;
    }
    else
    {
        gpu.MemTotalMiB = 0;
        gpu.MemUsedMiB = 0;
    }

    if (!card.GttTotalPath.isEmpty())
    {
        bool okTotal = false;
        bool okUsed = false;
        const qint64 total = Misc::ReadFile(card.GttTotalPath).trimmed().toLongLong(&okTotal);
        const qint64 used = Misc::ReadFile(card.GttUsedPath).trimmed().toLongLong(&okUsed);
        gpu.SharedMemTotalMiB = okTotal ? total / (1024LL * 1024LL) : 0;
        gpu.SharedMemUsedMiB = okUsed ? used / (1024LL * 1024LL) : 0;
    }
    else
    {
        gpu.SharedMemTotalMiB = 0;
        gpu.SharedMemUsedMiB = 0;
    }

    const double memPct = gpu.MemTotalMiB > 0
                              ? static_cast<double>(gpu.MemUsedMiB) / static_cast<double>(gpu.MemTotalMiB) * 100.0
                              : 0.0;
    gpu.MemUsageHistory.Push(memPct);

    const double sharedPct = gpu.SharedMemTotalMiB > 0
                                 ? static_cast<double>(gpu.SharedMemUsedMiB) / static_cast<double>(gpu.SharedMemTotalMiB) * 100.0
                                 : 0.0;
    gpu.SharedMemHistory.Push(sharedPct);

    gpu.CopyTxHistory.Push(0.0);
    gpu.CopyRxHistory.Push(0.0);

    QSet<QString> seenEngineKeys;
    auto addEngine = [&](const QString &key, const QString &label, double pct)
    {
        GPU::GPUEngineInfo *engine = gpu.FindEngine(key);
        if (!engine)
        {
            auto newEngine = std::make_unique<GPU::GPUEngineInfo>();
            newEngine->Key = key;
            newEngine->Label = label;
            gpu.Engines.push_back(std::move(newEngine));
            engine = gpu.Engines.back().get();
        }
        engine->Pct = qBound(0.0, pct, 100.0);
        engine->History.Push(engine->Pct);
        seenEngineKeys.insert(key);
    };

    if (!card.BusyPath.isEmpty())
        addEngine(QStringLiteral("gfx"), QStringLiteral("GFX"), gpu.UtilPct);

    for (const auto &enginePath : std::as_const(card.EngineBusyPaths))
    {
        bool ok = false;
        const int pct = Misc::ReadFile(enginePath.second).trimmed().toInt(&ok);
        addEngine(enginePath.first, enginePath.first.toUpper(), ok ? static_cast<double>(pct) : 0.0);
    }

    if (!card.RenderNodePath.isEmpty())
    {
        const QHash<QString, qint64> curNs = this->scanFdInfoEngines(card);
        QSet<QString> sysFsKeys;
        sysFsKeys.insert(QStringLiteral("gfx"));
        for (const auto &enginePath : std::as_const(card.EngineBusyPaths))
            sysFsKeys.insert(enginePath.first);

        for (auto it = curNs.cbegin(); it != curNs.cend(); ++it)
        {
            if (sysFsKeys.contains(it.key()))
                continue;

            double pct = 0.0;
            if (fdInfoElapsedNs > 0 && gpu.PrevFDInfoEngineNs.contains(it.key()))
            {
                const qint64 delta = it.value() - gpu.PrevFDInfoEngineNs.value(it.key());
                pct = static_cast<double>(delta) / static_cast<double>(fdInfoElapsedNs) * 100.0;
            }

            QString label = it.key();
            for (int i = 0; i < label.size(); ++i)
            {
                if (i == 0 || label[i - 1] == QLatin1Char('-'))
                    label[i] = label[i].toUpper();
            }
            addEngine(it.key(), label, pct);
        }
        gpu.PrevFDInfoEngineNs = curNs;
    }

    zeroMissingEngines(gpu, seenEngineKeys);
    return true;
}

bool GpuDrmBackend::sampleIntelCard(DRMCard &card, GPU::GPUInfo &gpu, qint64 fdInfoElapsedNs)
{
    gpu.TemperatureC = -1;
    gpu.CoreClockMHz = -1;
    gpu.PowerUsageW = -1.0;
    gpu.UtilPct = 0.0;
    gpu.CopyTxBps = 0.0;
    gpu.CopyRxBps = 0.0;

    if (!card.TempPath.isEmpty())
    {
        bool ok = false;
        const int milliC = Misc::ReadFile(card.TempPath).trimmed().toInt(&ok);
        gpu.TemperatureC = ok ? milliC / 1000 : -1;
    }

    if (!card.VramTotalPath.isEmpty())
    {
        bool okTotal = false;
        bool okUsed = false;
        const qint64 total = Misc::ReadFile(card.VramTotalPath).trimmed().toLongLong(&okTotal);
        const qint64 used = Misc::ReadFile(card.VramUsedPath).trimmed().toLongLong(&okUsed);
        gpu.MemTotalMiB = okTotal ? total / (1024LL * 1024LL) : 0;
        gpu.MemUsedMiB = okUsed ? used / (1024LL * 1024LL) : 0;
    }
    else
    {
        gpu.MemTotalMiB = 0;
        gpu.MemUsedMiB = 0;
    }

    if (!card.GttTotalPath.isEmpty())
    {
        bool okTotal = false;
        bool okUsed = false;
        const qint64 total = Misc::ReadFile(card.GttTotalPath).trimmed().toLongLong(&okTotal);
        const qint64 used = Misc::ReadFile(card.GttUsedPath).trimmed().toLongLong(&okUsed);
        gpu.SharedMemTotalMiB = okTotal ? total / (1024LL * 1024LL) : 0;
        gpu.SharedMemUsedMiB = okUsed ? used / (1024LL * 1024LL) : 0;
    }
    else
    {
        gpu.SharedMemTotalMiB = 0;
        gpu.SharedMemUsedMiB = 0;
    }

    QSet<QString> seenEngineKeys;
    auto addEngine = [&](const QString &key, const QString &label, double pct)
    {
        GPU::GPUEngineInfo *engine = gpu.FindEngine(key);
        if (!engine)
        {
            auto newEngine = std::make_unique<GPU::GPUEngineInfo>();
            newEngine->Key = key;
            newEngine->Label = label;
            gpu.Engines.push_back(std::move(newEngine));
            engine = gpu.Engines.back().get();
        }
        engine->Pct = qBound(0.0, pct, 100.0);
        engine->History.Push(engine->Pct);
        seenEngineKeys.insert(key);
    };

    QHash<QString, qint64> curNs;
    if (!card.RenderNodePath.isEmpty())
        curNs = this->scanFdInfoEngines(card);

    double totalUtil = 0.0;
    for (auto it = curNs.cbegin(); it != curNs.cend(); ++it)
    {
        double pct = 0.0;
        if (fdInfoElapsedNs > 0 && gpu.PrevFDInfoEngineNs.contains(it.key()))
        {
            const qint64 delta = it.value() - gpu.PrevFDInfoEngineNs.value(it.key());
            pct = static_cast<double>(delta) / static_cast<double>(fdInfoElapsedNs) * 100.0;
        }

        QString label;
        QString key = it.key();
        if (key == QLatin1String("rcs"))
            label = QStringLiteral("渲染");
        else if (key == QLatin1String("ccs"))
            label = QStringLiteral("计算");
        else if (key.startsWith(QLatin1String("ccs")))
            label = QStringLiteral("计算 ") + key.mid(3);
        else if (key.startsWith(QLatin1String("vcs")))
            label = QStringLiteral("视频");
        else if (key.startsWith(QLatin1String("vecs")))
            label = QStringLiteral("视频增强");
        else if (key.startsWith(QLatin1String("bcs")))
            label = QStringLiteral("复制");
        else
            label = key.toUpper();

        addEngine(key, label, pct);
        totalUtil += pct;
    }

    gpu.PrevFDInfoEngineNs = curNs;

    if (!card.BusyPath.isEmpty())
    {
        bool ok = false;
        const int pct = Misc::ReadFile(card.BusyPath).trimmed().toInt(&ok);
        gpu.UtilPct = ok ? qBound(0.0, static_cast<double>(pct), 100.0) : qBound(0.0, totalUtil, 100.0);
    }
    else
    {
        gpu.UtilPct = qBound(0.0, totalUtil, 100.0);
    }
    gpu.UtilHistory.Push(gpu.UtilPct);

    const double memPct = gpu.MemTotalMiB > 0
                              ? static_cast<double>(gpu.MemUsedMiB) / static_cast<double>(gpu.MemTotalMiB) * 100.0
                              : 0.0;
    gpu.MemUsageHistory.Push(memPct);

    const double sharedPct = gpu.SharedMemTotalMiB > 0
                                 ? static_cast<double>(gpu.SharedMemUsedMiB) / static_cast<double>(gpu.SharedMemTotalMiB) * 100.0
                                 : 0.0;
    gpu.SharedMemHistory.Push(sharedPct);

    gpu.CopyTxHistory.Push(0.0);
    gpu.CopyRxHistory.Push(0.0);

    zeroMissingEngines(gpu, seenEngineKeys);
    return true;
}

QHash<QString, qint64> GpuDrmBackend::scanFdInfoEngines(DRMCard &card)
{
    QHash<QString, qint64> totals;
    QSet<int> seenClients;

    auto parseFdInfo = [&](const QString &infoPath) -> bool
    {
        const QString content = Misc::ReadFile(infoPath);
        if (content.isEmpty())
            return false;

        if (!content.contains(QLatin1String("drm-pdev:\t") + card.ID)
            && !content.contains(QLatin1String("drm-pdev: ") + card.ID))
            return false;

        int clientId = -1;
        for (const auto &line : QStringView(content).split('\n'))
        {
            if (line.startsWith(QLatin1String("drm-client-id:")))
            {
                clientId = line.mid(line.indexOf(':') + 1).trimmed().toInt();
                break;
            }
        }
        if (clientId >= 0 && seenClients.contains(clientId))
            return false;
        if (clientId >= 0)
            seenClients.insert(clientId);

        bool found = false;
        for (const auto &line : QStringView(content).split('\n'))
        {
            if (!line.startsWith(QLatin1String("drm-engine-")))
                continue;
            const int colonPos = line.indexOf(':');
            if (colonPos < 0)
                continue;
            const QString key = line.mid(11, colonPos - 11).toString();
            const QStringView valStr = line.mid(colonPos + 1).trimmed();
            const int spacePos = valStr.indexOf(QLatin1Char(' '));
            const qint64 ns = (spacePos > 0 ? valStr.left(spacePos) : valStr).toLongLong();
            totals[key] += ns;
            found = true;
        }
        return found;
    };

    const bool fullRescan = (this->m_fdInfoRescanCounter % 5 == 1) || card.CachedFDInfoPaths.isEmpty();

    if (!fullRescan)
    {
        QStringList stillValid;
        for (const QString &path : std::as_const(card.CachedFDInfoPaths))
        {
            if (parseFdInfo(path))
                stillValid.append(path);
        }
        card.CachedFDInfoPaths = stillValid;
        return totals;
    }

    QStringList newCache;
    const QDir procDir(QStringLiteral("/proc"));
    const QStringList pids = procDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

    for (const QString &pidEntry : pids)
    {
        bool ok = false;
        pidEntry.toInt(&ok);
        if (!ok)
            continue;

        const QString fdDirPath = QStringLiteral("/proc/") + pidEntry + QStringLiteral("/fd");
        const QDir fdDir(fdDirPath);
        if (!fdDir.exists())
            continue;

        const QStringList fdEntries = fdDir.entryList(QDir::NoDotAndDotDot);
        for (const QString &fdNum : fdEntries)
        {
            const QString linkPath = fdDirPath + QLatin1Char('/') + fdNum;
            char buf[PATH_MAX];
            const ssize_t len = ::readlink(linkPath.toLocal8Bit().constData(), buf, sizeof(buf) - 1);
            if (len <= 0)
                continue;
            buf[len] = '\0';
            const QByteArray target(buf, static_cast<int>(len));

            if (target != card.RenderNodePath.toLatin1() && target != card.CardNodePath.toLatin1())
                continue;

            const QString infoPath = QStringLiteral("/proc/") + pidEntry + QStringLiteral("/fdinfo/") + fdNum;
            if (parseFdInfo(infoPath))
                newCache.append(infoPath);
        }
    }

    card.CachedFDInfoPaths = newCache;
    return totals;
}
