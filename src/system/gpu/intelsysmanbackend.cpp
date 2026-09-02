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

#include "intelsysmanbackend.h"
#include "../../logger.h"
#include "../../misc.h"

#include <QDir>
#include <QSet>
#include <QSysInfo>
#include <dlfcn.h>
#include <limits.h>
#include <unistd.h>

namespace
{
    using ZeResult = int32_t;
    using ZeBool = uint32_t;
    using ZesDriverHandle = void *;
    using ZesDeviceHandle = void *;
    using ZesEngineHandle = void *;
    using ZesMemHandle = void *;
    using ZesFreqHandle = void *;
    using ZesTempHandle = void *;
    using ZesPwrHandle = void *;

    static constexpr ZeResult ZE_RESULT_SUCCESS = 0;

    // Sysman 1.11+ structure type values (hex). Older sequential values were
    // renumbered; passing stale constants corrupts the properties the driver writes.
    static constexpr uint32_t ZES_STRUCTURE_TYPE_PCI_PROPERTIES = 0x2;
    static constexpr uint32_t ZES_STRUCTURE_TYPE_ENGINE_PROPERTIES = 0x5;
    static constexpr uint32_t ZES_STRUCTURE_TYPE_FREQ_PROPERTIES = 0x9;
    static constexpr uint32_t ZES_STRUCTURE_TYPE_FREQ_STATE = 0x1b;
    static constexpr uint32_t ZES_STRUCTURE_TYPE_MEM_PROPERTIES = 0xb;
    static constexpr uint32_t ZES_STRUCTURE_TYPE_MEM_STATE = 0x1e;
    static constexpr uint32_t ZES_STRUCTURE_TYPE_TEMP_PROPERTIES = 0x14;
    static constexpr uint32_t ZES_STRUCTURE_TYPE_POWER_PROPERTIES = 0xd;

    static constexpr uint32_t ZES_MEM_LOC_SYSTEM = 0;
    static constexpr uint32_t ZES_MEM_LOC_DEVICE = 1;

    static constexpr uint32_t ZES_FREQ_DOMAIN_GPU = 0;
    static constexpr uint32_t ZES_FREQ_DOMAIN_MEMORY = 1;
    static constexpr uint32_t ZES_FREQ_DOMAIN_MEDIA = 2;

    static constexpr uint32_t ZES_ENGINE_GROUP_ALL = 0;
    static constexpr uint32_t ZES_ENGINE_GROUP_COMPUTE_ALL = 1;
    static constexpr uint32_t ZES_ENGINE_GROUP_MEDIA_ALL = 2;
    static constexpr uint32_t ZES_ENGINE_GROUP_COPY_ALL = 3;
    static constexpr uint32_t ZES_ENGINE_GROUP_COMPUTE_SINGLE = 4;
    static constexpr uint32_t ZES_ENGINE_GROUP_RENDER_SINGLE = 5;
    static constexpr uint32_t ZES_ENGINE_GROUP_MEDIA_DECODE_SINGLE = 6;
    static constexpr uint32_t ZES_ENGINE_GROUP_MEDIA_ENCODE_SINGLE = 7;
    static constexpr uint32_t ZES_ENGINE_GROUP_COPY_SINGLE = 8;
    static constexpr uint32_t ZES_ENGINE_GROUP_MEDIA_ENHANCEMENT_SINGLE = 9;
    static constexpr uint32_t ZES_ENGINE_GROUP_3D_SINGLE = 10;
    static constexpr uint32_t ZES_ENGINE_GROUP_3D_RENDER_COMPUTE_ALL = 11;
    static constexpr uint32_t ZES_ENGINE_GROUP_RENDER_ALL = 12;
    static constexpr uint32_t ZES_ENGINE_GROUP_3D_ALL = 13;

    static constexpr uint32_t ZES_TEMP_SENSORS_GLOBAL = 0;
    static constexpr uint32_t ZES_TEMP_SENSORS_GPU = 1;
    static constexpr uint32_t ZES_TEMP_SENSORS_MEMORY = 2;
    static constexpr uint32_t ZES_TEMP_SENSORS_GPU_BOARD = 6;

    struct ZesPciAddress
    {
        uint32_t domain;
        uint32_t bus;
        uint32_t device;
        uint32_t function;
    };

    struct ZesPciSpeed
    {
        int32_t gen;
        int32_t width;
        int64_t maxBandwidth;
    };

    struct ZesPciProperties
    {
        uint32_t stype;
        void *pNext;
        ZesPciAddress address;
        ZesPciSpeed maxSpeed;
        ZeBool haveBandwidthCounters;
        ZeBool havePacketCounters;
        ZeBool haveReplayCounters;
    };

    struct ZesPciStats
    {
        uint64_t timestamp;
        uint64_t replayCounter;
        uint64_t packetCounter;
        uint64_t rxCounter;
        uint64_t txCounter;
        ZesPciSpeed speed;
    };

    struct ZesEngineProperties
    {
        uint32_t stype;
        void *pNext;
        uint32_t type;
        ZeBool onSubdevice;
        uint32_t subdeviceId;
    };

    struct ZesEngineStats
    {
        uint64_t activeTime;
        uint64_t timestamp;
    };

    struct ZesMemProperties
    {
        uint32_t stype;
        void *pNext;
        uint32_t type;
        ZeBool onSubdevice;
        uint32_t subdeviceId;
        uint32_t location;
        uint64_t physicalSize;
        int32_t busWidth;
        int32_t numChannels;
    };

    struct ZesMemState
    {
        uint32_t stype;
        const void *pNext;
        uint32_t health;
        uint64_t free;
        uint64_t size;
    };

    struct ZesFreqProperties
    {
        uint32_t stype;
        void *pNext;
        uint32_t type;
        ZeBool onSubdevice;
        uint32_t subdeviceId;
        ZeBool canControl;
        ZeBool isThrottleEventSupported;
        double min;
        double max;
    };

    struct ZesFreqState
    {
        uint32_t stype;
        const void *pNext;
        double currentVoltage;
        double request;
        double tdp;
        double efficient;
        double actual;
        uint32_t throttleReasons;
    };

    struct ZesTempProperties
    {
        uint32_t stype;
        void *pNext;
        uint32_t type;
        ZeBool onSubdevice;
        uint32_t subdeviceId;
        double maxTemperature;
        ZeBool isCriticalTempSupported;
        ZeBool isThreshold1Supported;
        ZeBool isThreshold2Supported;
    };

    struct ZesPowerProperties
    {
        uint32_t stype;
        void *pNext;
        ZeBool onSubdevice;
        uint32_t subdeviceId;
        ZeBool canControl;
        ZeBool isEnergyThresholdSupported;
        int32_t defaultLimit;
        int32_t minLimit;
        int32_t maxLimit;
    };

    struct ZesPowerEnergyCounter
    {
        uint64_t energy;
        uint64_t timestamp;
    };

    using FnZesInit = ZeResult (*)(uint32_t);
    using FnZesDriverGet = ZeResult (*)(uint32_t *, ZesDriverHandle *);
    using FnZesDeviceGet = ZeResult (*)(ZesDriverHandle, uint32_t *, ZesDeviceHandle *);
    using FnZesDevicePciGetProperties = ZeResult (*)(ZesDeviceHandle, ZesPciProperties *);
    using FnZesDevicePciGetStats = ZeResult (*)(ZesDeviceHandle, ZesPciStats *);
    using FnZesDeviceEnumEngineGroups = ZeResult (*)(ZesDeviceHandle, uint32_t *, ZesEngineHandle *);
    using FnZesEngineGetProperties = ZeResult (*)(ZesEngineHandle, ZesEngineProperties *);
    using FnZesEngineGetActivity = ZeResult (*)(ZesEngineHandle, ZesEngineStats *);
    using FnZesDeviceEnumMemoryModules = ZeResult (*)(ZesDeviceHandle, uint32_t *, ZesMemHandle *);
    using FnZesMemoryGetProperties = ZeResult (*)(ZesMemHandle, ZesMemProperties *);
    using FnZesMemoryGetState = ZeResult (*)(ZesMemHandle, ZesMemState *);
    using FnZesDeviceEnumFrequencyDomains = ZeResult (*)(ZesDeviceHandle, uint32_t *, ZesFreqHandle *);
    using FnZesFrequencyGetProperties = ZeResult (*)(ZesFreqHandle, ZesFreqProperties *);
    using FnZesFrequencyGetState = ZeResult (*)(ZesFreqHandle, ZesFreqState *);
    using FnZesDeviceEnumTemperatureSensors = ZeResult (*)(ZesDeviceHandle, uint32_t *, ZesTempHandle *);
    using FnZesTemperatureGetProperties = ZeResult (*)(ZesTempHandle, ZesTempProperties *);
    using FnZesTemperatureGetState = ZeResult (*)(ZesTempHandle, double *);
    using FnZesDeviceEnumPowerDomains = ZeResult (*)(ZesDeviceHandle, uint32_t *, ZesPwrHandle *);
    using FnZesPowerGetProperties = ZeResult (*)(ZesPwrHandle, ZesPowerProperties *);
    using FnZesPowerGetEnergyCounter = ZeResult (*)(ZesPwrHandle, ZesPowerEnergyCounter *);
    using FnZesDriverGetExtensionProperties = ZeResult (*)(ZesDriverHandle, uint32_t *, void *);
    using FnZeInit = ZeResult (*)(uint32_t);

    FnZesInit pZesInit = nullptr;
    FnZesDriverGet pZesDriverGet = nullptr;
    FnZesDeviceGet pZesDeviceGet = nullptr;
    FnZesDevicePciGetProperties pZesDevicePciGetProperties = nullptr;
    FnZesDevicePciGetStats pZesDevicePciGetStats = nullptr;
    FnZesDeviceEnumEngineGroups pZesDeviceEnumEngineGroups = nullptr;
    FnZesEngineGetProperties pZesEngineGetProperties = nullptr;
    FnZesEngineGetActivity pZesEngineGetActivity = nullptr;
    FnZesDeviceEnumMemoryModules pZesDeviceEnumMemoryModules = nullptr;
    FnZesMemoryGetProperties pZesMemoryGetProperties = nullptr;
    FnZesMemoryGetState pZesMemoryGetState = nullptr;
    FnZesDeviceEnumFrequencyDomains pZesDeviceEnumFrequencyDomains = nullptr;
    FnZesFrequencyGetProperties pZesFrequencyGetProperties = nullptr;
    FnZesFrequencyGetState pZesFrequencyGetState = nullptr;
    FnZesDeviceEnumTemperatureSensors pZesDeviceEnumTemperatureSensors = nullptr;
    FnZesTemperatureGetProperties pZesTemperatureGetProperties = nullptr;
    FnZesTemperatureGetState pZesTemperatureGetState = nullptr;
    FnZesDeviceEnumPowerDomains pZesDeviceEnumPowerDomains = nullptr;
    FnZesPowerGetProperties pZesPowerGetProperties = nullptr;
    FnZesPowerGetEnergyCounter pZesPowerGetEnergyCounter = nullptr;
    FnZeInit pZeInit = nullptr;

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

    QString pciIdFromAddress(const ZesPciAddress &address)
    {
        return QStringLiteral("%1:%2:%3.%4")
            .arg(address.domain, 4, 16, QLatin1Char('0'))
            .arg(address.bus, 2, 16, QLatin1Char('0'))
            .arg(address.device, 2, 16, QLatin1Char('0'))
            .arg(address.function, 1, 16, QLatin1Char('0'));
    }

    QString engineLabel(uint32_t type)
    {
        switch (type)
        {
            case ZES_ENGINE_GROUP_COMPUTE_ALL:
            case ZES_ENGINE_GROUP_COMPUTE_SINGLE:
                return QStringLiteral("Compute");
            case ZES_ENGINE_GROUP_MEDIA_ALL:
                return QStringLiteral("Media");
            case ZES_ENGINE_GROUP_COPY_ALL:
            case ZES_ENGINE_GROUP_COPY_SINGLE:
                return QStringLiteral("Copy");
            case ZES_ENGINE_GROUP_RENDER_SINGLE:
            case ZES_ENGINE_GROUP_RENDER_ALL:
                return QStringLiteral("Render");
            case ZES_ENGINE_GROUP_MEDIA_DECODE_SINGLE:
                return QStringLiteral("Video Decode");
            case ZES_ENGINE_GROUP_MEDIA_ENCODE_SINGLE:
                return QStringLiteral("Video Encode");
            case ZES_ENGINE_GROUP_MEDIA_ENHANCEMENT_SINGLE:
                return QStringLiteral("Video Enhance");
            case ZES_ENGINE_GROUP_3D_SINGLE:
            case ZES_ENGINE_GROUP_3D_ALL:
                return QStringLiteral("3D");
            case ZES_ENGINE_GROUP_3D_RENDER_COMPUTE_ALL:
                return QStringLiteral("3D/Render");
            case ZES_ENGINE_GROUP_ALL:
            default:
                return QStringLiteral("All");
        }
    }

    QString engineKey(uint32_t type, uint32_t subdeviceId, ZeBool onSubdevice)
    {
        return QStringLiteral("%1:%2:%3")
            .arg(type)
            .arg(onSubdevice ? 1 : 0)
            .arg(subdeviceId);
    }

    QString detectDriverVersionFromBdf(const QString &bdf)
    {
        QString driverName = Misc::FileNameFromSymlink(QStringLiteral("/sys/bus/pci/devices/") + bdf + QStringLiteral("/driver/module"));
        if (driverName.isEmpty())
            driverName = Misc::FileNameFromSymlink(QStringLiteral("/sys/bus/pci/devices/") + bdf + QStringLiteral("/driver"));
        if (!driverName.isEmpty())
        {
            const QString version = Misc::ReadFile(QStringLiteral("/sys/module/") + driverName + QStringLiteral("/version")).trimmed();
            if (!version.isEmpty())
                return version;
        }
        return QSysInfo::kernelVersion();
    }

    QString fdEngineLabel(const QString &key)
    {
        if (key == QLatin1String("rcs"))
            return QStringLiteral("Render");
        if (key.startsWith(QLatin1String("ccs")))
            return QStringLiteral("Compute") + (key.size() > 3 ? QLatin1Char(' ') + key.mid(3) : QString());
        if (key.startsWith(QLatin1String("vcs")))
            return QStringLiteral("Video");
        if (key.startsWith(QLatin1String("vecs")))
            return QStringLiteral("Video Enhance");
        if (key.startsWith(QLatin1String("bcs")))
            return QStringLiteral("Copy");
        return key.toUpper();
    }

    QString renderNodeForBdf(const QString &bdf)
    {
        const QDir drmDir(QStringLiteral("/sys/bus/pci/devices/") + bdf + QStringLiteral("/drm"));
        const QStringList renderNodes = drmDir.entryList({QStringLiteral("renderD[0-9]*")}, QDir::Dirs);
        return renderNodes.isEmpty() ? QString() : QStringLiteral("/dev/dri/") + renderNodes.first();
    }

    //! Reads the GPU package temperature from hwmon sysfs (fallback for sysman,
    //! which has no sensors when PMT telemetry is unavailable). Prefers the
    //! lowest-numbered sensor; on xe that is temp2_input ("pkg").
    int readHwmonTemperatureC(const QString &bdf)
    {
        const QDir hwmonRoot(QStringLiteral("/sys/bus/pci/devices/") + bdf + QStringLiteral("/hwmon"));
        const QStringList hwmonDirs = hwmonRoot.entryList({QStringLiteral("hwmon[0-9]*")}, QDir::Dirs);

        int bestSensor = INT_MAX;
        int bestMilliC = -1;

        for (const QString &hwmonName : hwmonDirs)
        {
            const QDir dir(hwmonRoot.filePath(hwmonName));
            const QStringList inputs = dir.entryList({QStringLiteral("temp[0-9]*_input")}, QDir::Files);
            for (const QString &input : inputs)
            {
                // "temp2_input" -> "2"
                const QString numStr = input.mid(4, input.size() - 4 - QStringLiteral("_input").size());
                bool ok = false;
                const int sensor = numStr.toInt(&ok);
                if (!ok || sensor >= bestSensor)
                    continue;

                bool okRead = false;
                const int milliC = Misc::ReadFile(dir.filePath(input)).trimmed().toInt(&okRead);
                if (!okRead)
                    continue;

                bestSensor = sensor;
                bestMilliC = milliC;
            }
        }

        return bestMilliC > 0 ? bestMilliC / 1000 : -1;
    }

    //! Parses one client fdinfo file into per-engine-class counters.
    //! Handles legacy i915 "drm-engine-<key>:" (nanoseconds busy) and xe
    //! "drm-cycles-<key>:" / "drm-total-cycles-<key>:" (GPU clock domain).
    bool parseFdInfoFile(const QString &infoPath, const QString &pdevId, QHash<QString, GpuIntelSysmanBackend::FdEngineSnapshot> &out)
    {
        const QString content = Misc::ReadFile(infoPath);
        if (content.isEmpty())
            return false;

        if (!content.contains(QLatin1String("drm-pdev:\t") + pdevId)
            && !content.contains(QLatin1String("drm-pdev: ") + pdevId))
            return false;

        bool found = false;
        for (const auto &line : QStringView(content).split('\n'))
        {
            if (line.startsWith(QLatin1String("drm-engine-capacity-")))
            {
                // Static metadata: number of parallel engines in the class
                // (e.g. vcs=2, ccs=4). Not a busy counter — deliberately ignored.
                continue;
            }
            else if (line.startsWith(QLatin1String("drm-engine-")))
            {
                // "drm-engine-rcs: <busy_ns> <timestamp>"
                const int colonPos = line.indexOf(':');
                if (colonPos < 0)
                    continue;
                const QString key = line.mid(11, colonPos - 11).toString();
                const QStringView valStr = line.mid(colonPos + 1).trimmed();
                const int spacePos = valStr.indexOf(QLatin1Char(' '));
                out[key].busyNs += (spacePos > 0 ? valStr.left(spacePos) : valStr).toULongLong();
                found = true;
            }
            else if (line.startsWith(QLatin1String("drm-total-cycles-")))
            {
                // "drm-total-cycles-rcs: <cycles>" — shared GT clock reference,
                // identical for every client; keep the maximum seen
                const int colonPos = line.indexOf(':');
                if (colonPos < 0)
                    continue;
                const QString key = line.mid(17, colonPos - 17).toString();
                out[key].totalCycles = qMax(out[key].totalCycles, line.mid(colonPos + 1).trimmed().toULongLong());
                found = true;
            }
            else if (line.startsWith(QLatin1String("drm-cycles-")))
            {
                // "drm-cycles-rcs: <busy_cycles>" — summed over clients
                const int colonPos = line.indexOf(':');
                if (colonPos < 0)
                    continue;
                const QString key = line.mid(11, colonPos - 11).toString();
                out[key].cycles += line.mid(colonPos + 1).trimmed().toULongLong();
                found = true;
            }
        }
        return found;
    }

    //! Scans every /proc/*/fdinfo of clients holding the given render node,
    //! summing per-engine-class busy counters across clients.
    QHash<QString, GpuIntelSysmanBackend::FdEngineSnapshot> scanFdInfoEngines(const QString &renderNodePath, const QString &pdevId,
                                                                              QStringList &cachedPaths, bool fullRescan)
    {
        QHash<QString, GpuIntelSysmanBackend::FdEngineSnapshot> totals;

        if (!fullRescan)
        {
            QStringList stillValid;
            for (const QString &path : std::as_const(cachedPaths))
            {
                if (parseFdInfoFile(path, pdevId, totals))
                    stillValid.append(path);
            }
            cachedPaths = stillValid;
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

                if (target != renderNodePath.toLatin1())
                    continue;

                const QString infoPath = QStringLiteral("/proc/") + pidEntry + QStringLiteral("/fdinfo/") + fdNum;
                if (parseFdInfoFile(infoPath, pdevId, totals))
                    newCache.append(infoPath);
            }
        }

        cachedPaths = newCache;
        return totals;
    }
}

GpuIntelSysmanBackend::~GpuIntelSysmanBackend()
{
    this->unload();
}

void GpuIntelSysmanBackend::Detect()
{
    this->unload();

    this->m_libHandle = ::dlopen("libze_loader.so.1", RTLD_LAZY | RTLD_LOCAL);
    if (!this->m_libHandle)
    {
        LOG_DEBUG("Intel Sysman scan: unable to locate libze_loader.so.1, trying fallback to libze_loader.so");
        this->m_libHandle = ::dlopen("libze_loader.so", RTLD_LAZY | RTLD_LOCAL);
    }

    if (!this->m_libHandle)
    {
        LOG_DEBUG("Intel Sysman scan: no Level Zero loader detected");
        return;
    }

    pZesInit = reinterpret_cast<FnZesInit>(::dlsym(this->m_libHandle, "zesInit"));
    pZesDriverGet = reinterpret_cast<FnZesDriverGet>(::dlsym(this->m_libHandle, "zesDriverGet"));
    pZesDeviceGet = reinterpret_cast<FnZesDeviceGet>(::dlsym(this->m_libHandle, "zesDeviceGet"));
    pZesDevicePciGetProperties = reinterpret_cast<FnZesDevicePciGetProperties>(::dlsym(this->m_libHandle, "zesDevicePciGetProperties"));
    pZesDevicePciGetStats = reinterpret_cast<FnZesDevicePciGetStats>(::dlsym(this->m_libHandle, "zesDevicePciGetStats"));
    pZesDeviceEnumEngineGroups = reinterpret_cast<FnZesDeviceEnumEngineGroups>(::dlsym(this->m_libHandle, "zesDeviceEnumEngineGroups"));
    pZesEngineGetProperties = reinterpret_cast<FnZesEngineGetProperties>(::dlsym(this->m_libHandle, "zesEngineGetProperties"));
    pZesEngineGetActivity = reinterpret_cast<FnZesEngineGetActivity>(::dlsym(this->m_libHandle, "zesEngineGetActivity"));
    pZesDeviceEnumMemoryModules = reinterpret_cast<FnZesDeviceEnumMemoryModules>(::dlsym(this->m_libHandle, "zesDeviceEnumMemoryModules"));
    pZesMemoryGetProperties = reinterpret_cast<FnZesMemoryGetProperties>(::dlsym(this->m_libHandle, "zesMemoryGetProperties"));
    pZesMemoryGetState = reinterpret_cast<FnZesMemoryGetState>(::dlsym(this->m_libHandle, "zesMemoryGetState"));
    pZesDeviceEnumFrequencyDomains = reinterpret_cast<FnZesDeviceEnumFrequencyDomains>(::dlsym(this->m_libHandle, "zesDeviceEnumFrequencyDomains"));
    pZesFrequencyGetProperties = reinterpret_cast<FnZesFrequencyGetProperties>(::dlsym(this->m_libHandle, "zesFrequencyGetProperties"));
    pZesFrequencyGetState = reinterpret_cast<FnZesFrequencyGetState>(::dlsym(this->m_libHandle, "zesFrequencyGetState"));
    pZesDeviceEnumTemperatureSensors = reinterpret_cast<FnZesDeviceEnumTemperatureSensors>(::dlsym(this->m_libHandle, "zesDeviceEnumTemperatureSensors"));
    pZesTemperatureGetProperties = reinterpret_cast<FnZesTemperatureGetProperties>(::dlsym(this->m_libHandle, "zesTemperatureGetProperties"));
    pZesTemperatureGetState = reinterpret_cast<FnZesTemperatureGetState>(::dlsym(this->m_libHandle, "zesTemperatureGetState"));
    pZesDeviceEnumPowerDomains = reinterpret_cast<FnZesDeviceEnumPowerDomains>(::dlsym(this->m_libHandle, "zesDeviceEnumPowerDomains"));
    pZesPowerGetProperties = reinterpret_cast<FnZesPowerGetProperties>(::dlsym(this->m_libHandle, "zesPowerGetProperties"));
    pZesPowerGetEnergyCounter = reinterpret_cast<FnZesPowerGetEnergyCounter>(::dlsym(this->m_libHandle, "zesPowerGetEnergyCounter"));
    pZeInit = reinterpret_cast<FnZeInit>(::dlsym(this->m_libHandle, "zeInit"));

    const bool symbolsOk = pZesInit && pZesDriverGet && pZesDeviceGet
                           && pZesDevicePciGetProperties && pZesDeviceEnumEngineGroups
                           && pZesEngineGetProperties && pZesEngineGetActivity
                           && pZesDeviceEnumMemoryModules && pZesMemoryGetProperties
                           && pZesMemoryGetState && pZesDeviceEnumFrequencyDomains
                           && pZesFrequencyGetProperties && pZesFrequencyGetState
                           && pZesDeviceEnumTemperatureSensors && pZesTemperatureGetProperties
                           && pZesTemperatureGetState && pZesDeviceEnumPowerDomains
                           && pZesPowerGetProperties && pZesPowerGetEnergyCounter;
    if (!symbolsOk)
    {
        LOG_DEBUG("Intel Sysman: required symbols not found, unloading");
        this->unload();
        return;
    }

    if (pZeInit)
        pZeInit(0);

    if (pZesInit(0) != ZE_RESULT_SUCCESS)
    {
        LOG_DEBUG("Intel Sysman: zesInit failed, unloading");
        this->unload();
        return;
    }

    uint32_t driverCount = 0;
    if (pZesDriverGet(&driverCount, nullptr) != ZE_RESULT_SUCCESS || driverCount == 0)
    {
        LOG_DEBUG("Intel Sysman: no sysman drivers found");
        this->unload();
        return;
    }

    std::vector<ZesDriverHandle> drivers(driverCount);
    if (pZesDriverGet(&driverCount, drivers.data()) != ZE_RESULT_SUCCESS)
    {
        LOG_DEBUG("Intel Sysman: failed to enumerate sysman drivers");
        this->unload();
        return;
    }

    bool hasGpu = false;
    for (ZesDriverHandle driver : drivers)
    {
        uint32_t deviceCount = 0;
        if (pZesDeviceGet(driver, &deviceCount, nullptr) == ZE_RESULT_SUCCESS && deviceCount > 0)
        {
            hasGpu = true;
            break;
        }
    }

    if (!hasGpu)
    {
        LOG_DEBUG("Intel Sysman: no devices found");
        this->unload();
        return;
    }

    this->m_available = true;
}

bool GpuIntelSysmanBackend::Sample(std::vector<std::unique_ptr<GPU::GPUInfo>> &gpus)
{
    if (!this->m_available)
        return false;

    const qint64 fdInfoElapsedNs = this->m_fdInfoTimerStarted ? this->m_fdInfoTimer.nsecsElapsed() : 0;

    uint32_t driverCount = 0;
    if (pZesDriverGet(&driverCount, nullptr) != ZE_RESULT_SUCCESS || driverCount == 0)
        return false;

    std::vector<ZesDriverHandle> drivers(driverCount);
    if (pZesDriverGet(&driverCount, drivers.data()) != ZE_RESULT_SUCCESS)
        return false;

    QSet<QString> seenIds;

    for (ZesDriverHandle driver : drivers)
    {
        uint32_t deviceCount = 0;
        if (pZesDeviceGet(driver, &deviceCount, nullptr) != ZE_RESULT_SUCCESS || deviceCount == 0)
            continue;

        std::vector<ZesDeviceHandle> devices(deviceCount);
        if (pZesDeviceGet(driver, &deviceCount, devices.data()) != ZE_RESULT_SUCCESS)
            continue;

        for (ZesDeviceHandle device : devices)
        {
            ZesPciProperties pciProps {};
            pciProps.stype = ZES_STRUCTURE_TYPE_PCI_PROPERTIES;
            pciProps.pNext = nullptr;
            if (pZesDevicePciGetProperties(device, &pciProps) != ZE_RESULT_SUCCESS)
                continue;

            const QString id = pciIdFromAddress(pciProps.address);
            GPU::GPUInfo &gpu = *findOrCreateGpu(gpus, id);
            gpu.ID = id;
            gpu.Name = QStringLiteral("Intel Graphics");
            gpu.DriverVersion = detectDriverVersionFromBdf(id);
            gpu.Backend = QStringLiteral("Intel Sysman");
            gpu.TemperatureC = -1;
            gpu.CoreClockMHz = -1;
            gpu.PowerUsageW = -1.0;
            gpu.UtilPct = 0.0;
            gpu.MemTotalMiB = 0;
            gpu.MemUsedMiB = 0;
            gpu.SharedMemTotalMiB = 0;
            gpu.SharedMemUsedMiB = 0;
            gpu.CopyTxBps = 0.0;
            gpu.CopyRxBps = 0.0;

            uint32_t engineCount = 0;
            if (pZesDeviceEnumEngineGroups(device, &engineCount, nullptr) == ZE_RESULT_SUCCESS && engineCount > 0)
            {
                std::vector<ZesEngineHandle> engines(engineCount);
                if (pZesDeviceEnumEngineGroups(device, &engineCount, engines.data()) == ZE_RESULT_SUCCESS)
                {
                    QSet<QString> seenEngineKeys;
                    double utilAccumulator = 0.0;
                    for (ZesEngineHandle engineHandle : engines)
                    {
                        ZesEngineProperties props {};
                        props.stype = ZES_STRUCTURE_TYPE_ENGINE_PROPERTIES;
                        props.pNext = nullptr;
                        if (pZesEngineGetProperties(engineHandle, &props) != ZE_RESULT_SUCCESS)
                            continue;

                        ZesEngineStats stats {};
                        if (pZesEngineGetActivity(engineHandle, &stats) != ZE_RESULT_SUCCESS)
                            continue;

                        const QString key = engineKey(props.type, props.subdeviceId, props.onSubdevice);
                        double pct = 0.0;
                        if (this->m_prevEngineByKey.contains(id + QLatin1Char('/') + key))
                        {
                            const Snapshot prev = this->m_prevEngineByKey.value(id + QLatin1Char('/') + key);
                            if (stats.timestamp > prev.timestamp)
                                pct = static_cast<double>(stats.activeTime - prev.value)
                                      / static_cast<double>(stats.timestamp - prev.timestamp) * 100.0;
                        }
                        this->m_prevEngineByKey.insert(id + QLatin1Char('/') + key, {stats.activeTime, stats.timestamp});

                        const QString label = engineLabel(props.type);
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

                        switch (props.type)
                        {
                            case ZES_ENGINE_GROUP_COMPUTE_ALL:
                            case ZES_ENGINE_GROUP_RENDER_ALL:
                            case ZES_ENGINE_GROUP_3D_RENDER_COMPUTE_ALL:
                            case ZES_ENGINE_GROUP_ALL:
                                utilAccumulator = qMax(utilAccumulator, pct);
                                break;
                            default:
                                break;
                        }
                    }
                    zeroMissingEngines(gpu, seenEngineKeys);
                    gpu.UtilPct = qBound(0.0, utilAccumulator, 100.0);
                }
            }
            else
            {
                // Sysman cannot enumerate engine groups here (non-root processes get
                // ZE_RESULT_ERROR_INSUFFICIENT_PERMISSIONS on Linux). Fall back to
                // per-client DRM fdinfo busy counters instead.
                this->sampleFdInfoEngines(gpu, id, fdInfoElapsedNs);
            }
            gpu.UtilHistory.Push(gpu.UtilPct);

            uint32_t memCount = 0;
            if (pZesDeviceEnumMemoryModules(device, &memCount, nullptr) == ZE_RESULT_SUCCESS && memCount > 0)
            {
                std::vector<ZesMemHandle> memories(memCount);
                if (pZesDeviceEnumMemoryModules(device, &memCount, memories.data()) == ZE_RESULT_SUCCESS)
                {
                    for (ZesMemHandle memory : memories)
                    {
                        ZesMemProperties props {};
                        props.stype = ZES_STRUCTURE_TYPE_MEM_PROPERTIES;
                        props.pNext = nullptr;
                        if (pZesMemoryGetProperties(memory, &props) != ZE_RESULT_SUCCESS)
                            continue;

                        ZesMemState state {};
                        state.stype = ZES_STRUCTURE_TYPE_MEM_STATE;
                        state.pNext = nullptr;
                        if (pZesMemoryGetState(memory, &state) != ZE_RESULT_SUCCESS)
                            continue;

                        const qint64 totalMiB = static_cast<qint64>(state.size / (1024ULL * 1024ULL));
                        const qint64 usedMiB = static_cast<qint64>((state.size - state.free) / (1024ULL * 1024ULL));
                        if (props.location == ZES_MEM_LOC_DEVICE)
                        {
                            gpu.MemTotalMiB += totalMiB;
                            gpu.MemUsedMiB += usedMiB;
                        }
                        else
                        {
                            gpu.SharedMemTotalMiB += totalMiB;
                            gpu.SharedMemUsedMiB += usedMiB;
                        }
                    }
                }
            }

            const double memPct = gpu.MemTotalMiB > 0
                                      ? static_cast<double>(gpu.MemUsedMiB) / static_cast<double>(gpu.MemTotalMiB) * 100.0
                                      : 0.0;
            gpu.MemUsageHistory.Push(memPct);
            const double sharedPct = gpu.SharedMemTotalMiB > 0
                                         ? static_cast<double>(gpu.SharedMemUsedMiB) / static_cast<double>(gpu.SharedMemTotalMiB) * 100.0
                                         : 0.0;
            gpu.SharedMemHistory.Push(sharedPct);

            uint32_t freqCount = 0;
            if (pZesDeviceEnumFrequencyDomains(device, &freqCount, nullptr) == ZE_RESULT_SUCCESS && freqCount > 0)
            {
                std::vector<ZesFreqHandle> freqs(freqCount);
                if (pZesDeviceEnumFrequencyDomains(device, &freqCount, freqs.data()) == ZE_RESULT_SUCCESS)
                {
                    for (ZesFreqHandle freqHandle : freqs)
                    {
                        ZesFreqProperties props {};
                        props.stype = ZES_STRUCTURE_TYPE_FREQ_PROPERTIES;
                        props.pNext = nullptr;
                        if (pZesFrequencyGetProperties(freqHandle, &props) != ZE_RESULT_SUCCESS)
                            continue;
                        if (props.type != ZES_FREQ_DOMAIN_GPU)
                            continue;

                        ZesFreqState state {};
                        state.stype = ZES_STRUCTURE_TYPE_FREQ_STATE;
                        state.pNext = nullptr;
                        if (pZesFrequencyGetState(freqHandle, &state) == ZE_RESULT_SUCCESS)
                            gpu.CoreClockMHz = state.actual >= 0.0 ? static_cast<int>(state.actual) : -1;
                        break;
                    }
                }
            }

            uint32_t tempCount = 0;
            if (pZesDeviceEnumTemperatureSensors(device, &tempCount, nullptr) == ZE_RESULT_SUCCESS && tempCount > 0)
            {
                std::vector<ZesTempHandle> temps(tempCount);
                if (pZesDeviceEnumTemperatureSensors(device, &tempCount, temps.data()) == ZE_RESULT_SUCCESS)
                {
                    double bestTemp = -1.0;
                    int bestRank = 100;
                    for (ZesTempHandle tempHandle : temps)
                    {
                        ZesTempProperties props {};
                        props.stype = ZES_STRUCTURE_TYPE_TEMP_PROPERTIES;
                        props.pNext = nullptr;
                        if (pZesTemperatureGetProperties(tempHandle, &props) != ZE_RESULT_SUCCESS)
                            continue;

                        double tempC = 0.0;
                        if (pZesTemperatureGetState(tempHandle, &tempC) != ZE_RESULT_SUCCESS)
                            continue;

                        int rank = 100;
                        switch (props.type)
                        {
                            case ZES_TEMP_SENSORS_GPU: rank = 0; break;
                            case ZES_TEMP_SENSORS_GLOBAL: rank = 1; break;
                            case ZES_TEMP_SENSORS_GPU_BOARD: rank = 2; break;
                            case ZES_TEMP_SENSORS_MEMORY: rank = 3; break;
                            default: break;
                        }
                        if (rank < bestRank || (rank == bestRank && tempC > bestTemp))
                        {
                            bestRank = rank;
                            bestTemp = tempC;
                        }
                    }
                    gpu.TemperatureC = bestTemp >= 0.0 ? static_cast<int>(bestTemp) : -1;
                }
            }

            if (gpu.TemperatureC < 0)
            {
                // Sysman exposes no temperature sensors when PMT telemetry is
                // unavailable (typical on xe without intel_vsec/PMT nodes). The
                // kernel hwmon interface always has the package sensor, so read it.
                const int hwmonTempC = readHwmonTemperatureC(id);
                if (hwmonTempC >= 0)
                {
                    gpu.TemperatureC = hwmonTempC;
                    this->setHwmonTempFallback(true);
                }
                else
                {
                    this->setHwmonTempFallback(false);
                }
            }
            else
            {
                this->setHwmonTempFallback(false);
            }

            uint32_t powerCount = 0;
            if (pZesDeviceEnumPowerDomains(device, &powerCount, nullptr) == ZE_RESULT_SUCCESS && powerCount > 0)
            {
                std::vector<ZesPwrHandle> powers(powerCount);
                if (pZesDeviceEnumPowerDomains(device, &powerCount, powers.data()) == ZE_RESULT_SUCCESS)
                {
                    for (ZesPwrHandle powerHandle : powers)
                    {
                        ZesPowerProperties props {};
                        props.stype = ZES_STRUCTURE_TYPE_POWER_PROPERTIES;
                        props.pNext = nullptr;
                        if (pZesPowerGetProperties(powerHandle, &props) != ZE_RESULT_SUCCESS || props.onSubdevice)
                            continue;

                        ZesPowerEnergyCounter energy {};
                        if (pZesPowerGetEnergyCounter(powerHandle, &energy) != ZE_RESULT_SUCCESS)
                            continue;

                        if (this->m_prevEnergyById.contains(id))
                        {
                            const Snapshot prev = this->m_prevEnergyById.value(id);
                            if (energy.timestamp > prev.timestamp)
                            {
                                const double joules = static_cast<double>(energy.energy - prev.value) / 1000000.0;
                                const double seconds = static_cast<double>(energy.timestamp - prev.timestamp) / 1000000.0;
                                if (seconds > 0.0)
                                    gpu.PowerUsageW = joules / seconds;
                            }
                        }
                        this->m_prevEnergyById.insert(id, {energy.energy, energy.timestamp});
                        break;
                    }
                }
            }

            if (pZesDevicePciGetStats && pciProps.haveBandwidthCounters)
            {
                ZesPciStats stats {};
                if (pZesDevicePciGetStats(device, &stats) == ZE_RESULT_SUCCESS)
                {
                    if (this->m_prevPciRxById.contains(id))
                    {
                        const Snapshot prevRx = this->m_prevPciRxById.value(id);
                        if (stats.timestamp > prevRx.timestamp)
                        {
                            const double seconds = static_cast<double>(stats.timestamp - prevRx.timestamp) / 1000000.0;
                            if (seconds > 0.0)
                                gpu.CopyRxBps = static_cast<double>(stats.rxCounter - prevRx.value) / seconds;
                        }
                    }
                    if (this->m_prevPciTxById.contains(id))
                    {
                        const Snapshot prevTx = this->m_prevPciTxById.value(id);
                        if (stats.timestamp > prevTx.timestamp)
                        {
                            const double seconds = static_cast<double>(stats.timestamp - prevTx.timestamp) / 1000000.0;
                            if (seconds > 0.0)
                                gpu.CopyTxBps = static_cast<double>(stats.txCounter - prevTx.value) / seconds;
                        }
                    }
                    this->m_prevPciRxById.insert(id, {stats.rxCounter, stats.timestamp});
                    this->m_prevPciTxById.insert(id, {stats.txCounter, stats.timestamp});
                }
            }
            Misc::PushHistoryAndUpdateMax(gpu.CopyTxHistory, gpu.CopyTxBps, gpu.MaxCopyBps);
            Misc::PushHistoryAndUpdateMax(gpu.CopyRxHistory, gpu.CopyRxBps, gpu.MaxCopyBps);

            seenIds.insert(id);
        }
    }

    for (const auto &gpu : gpus)
    {
        if (gpu->Backend != QLatin1String("Intel Sysman") || seenIds.contains(gpu->ID))
            continue;

        gpu->UtilPct = 0.0;
        gpu->TemperatureC = -1;
        gpu->CoreClockMHz = -1;
        gpu->PowerUsageW = -1.0;
        gpu->MemUsedMiB = 0;
        gpu->MemTotalMiB = 0;
        gpu->SharedMemUsedMiB = 0;
        gpu->SharedMemTotalMiB = 0;
        gpu->CopyTxBps = 0.0;
        gpu->CopyRxBps = 0.0;
        gpu->UtilHistory.Push(0.0);
        gpu->MemUsageHistory.Push(0.0);
        gpu->SharedMemHistory.Push(0.0);
        Misc::PushHistoryAndUpdateMax(gpu->CopyTxHistory, 0.0, gpu->MaxCopyBps);
        Misc::PushHistoryAndUpdateMax(gpu->CopyRxHistory, 0.0, gpu->MaxCopyBps);
        for (const auto &engine : gpu->Engines)
        {
            engine->Pct = 0.0;
            engine->History.Push(0.0);
        }
    }

    this->m_fdInfoTimer.start();
    this->m_fdInfoTimerStarted = true;

    return !seenIds.isEmpty();
}

void GpuIntelSysmanBackend::sampleFdInfoEngines(GPU::GPUInfo &gpu, const QString &bdf, qint64 intervalNs)
{
    if (this->m_fdRenderNode.isEmpty())
        this->m_fdRenderNode = renderNodeForBdf(bdf);

    if (this->m_fdRenderNode.isEmpty())
    {
        this->setFdEngineFallback(false);
        gpu.UtilPct = 0.0;
        return;
    }

    const bool fullRescan = (++this->m_fdInfoRescanCounter % 5 == 1) || this->m_fdInfoPaths.isEmpty();
    const QHash<QString, FdEngineSnapshot> cur = scanFdInfoEngines(this->m_fdRenderNode, bdf, this->m_fdInfoPaths, fullRescan);

    QSet<QString> seenEngineKeys;
    double bestPct = 0.0;

    for (auto it = cur.cbegin(); it != cur.cend(); ++it)
    {
        const QString &key = it.key();
        const FdEngineSnapshot &snap = it.value();
        const QString stateKey = bdf + QLatin1Char('/') + key;

        double pct = 0.0;
        if (this->m_prevFdEngines.contains(stateKey))
        {
            const FdEngineSnapshot &prev = this->m_prevFdEngines.value(stateKey);
            if (snap.totalCycles > 0 && snap.totalCycles > prev.totalCycles)
            {
                // xe: utilization = delta busy cycles / delta GPU clock-domain total
                const qint64 dBusy = static_cast<qint64>(snap.cycles) - static_cast<qint64>(prev.cycles);
                const qint64 dTotal = static_cast<qint64>(snap.totalCycles) - static_cast<qint64>(prev.totalCycles);
                if (dTotal > 0 && dBusy >= 0)
                    pct = static_cast<double>(dBusy) / static_cast<double>(dTotal) * 100.0;
            }
            else if (intervalNs > 0)
            {
                // i915: busy counters are already in nanoseconds
                const qint64 dBusy = static_cast<qint64>(snap.busyNs) - static_cast<qint64>(prev.busyNs);
                if (dBusy >= 0)
                    pct = static_cast<double>(dBusy) / static_cast<double>(intervalNs) * 100.0;
            }
        }
        this->m_prevFdEngines.insert(stateKey, snap);

        GPU::GPUEngineInfo *engine = gpu.FindEngine(key);
        if (!engine)
        {
            auto newEngine = std::make_unique<GPU::GPUEngineInfo>();
            newEngine->Key = key;
            newEngine->Label = fdEngineLabel(key);
            gpu.Engines.push_back(std::move(newEngine));
            engine = gpu.Engines.back().get();
        }
        engine->Pct = qBound(0.0, pct, 100.0);
        engine->History.Push(engine->Pct);
        seenEngineKeys.insert(key);
        bestPct = qMax(bestPct, pct);
    }

    zeroMissingEngines(gpu, seenEngineKeys);
    // Equivalent to the sysman ZES_ENGINE_GROUP_ALL aggregate: busiest engine class.
    gpu.UtilPct = qBound(0.0, bestPct, 100.0);
    this->setFdEngineFallback(true);
}

void GpuIntelSysmanBackend::setFdEngineFallback(bool active)
{
    if (this->m_fdEngineFallbackActive == active)
        return;

    this->m_fdEngineFallbackActive = active;
    if (active)
        LOG_DEBUG("Intel Sysman: engine activity unavailable, falling back to per-client DRM fdinfo counters");
}

void GpuIntelSysmanBackend::setHwmonTempFallback(bool active)
{
    if (this->m_hwmonTempFallbackActive == active)
        return;

    this->m_hwmonTempFallbackActive = active;
    if (active)
        LOG_DEBUG("Intel Sysman: no temperature sensors exposed, falling back to hwmon");
}

void GpuIntelSysmanBackend::unload()
{
    this->m_available = false;
    this->m_prevEnergyById.clear();
    this->m_prevPciRxById.clear();
    this->m_prevPciTxById.clear();
    this->m_prevEngineByKey.clear();
    this->m_prevFdEngines.clear();
    this->m_fdInfoPaths.clear();
    this->m_fdInfoRescanCounter = 0;
    this->m_fdInfoTimerStarted = false;
    this->m_fdEngineFallbackActive = false;
    this->m_hwmonTempFallbackActive = false;

    pZesInit = nullptr;
    pZesDriverGet = nullptr;
    pZesDeviceGet = nullptr;
    pZesDevicePciGetProperties = nullptr;
    pZesDevicePciGetStats = nullptr;
    pZesDeviceEnumEngineGroups = nullptr;
    pZesEngineGetProperties = nullptr;
    pZesEngineGetActivity = nullptr;
    pZesDeviceEnumMemoryModules = nullptr;
    pZesMemoryGetProperties = nullptr;
    pZesMemoryGetState = nullptr;
    pZesDeviceEnumFrequencyDomains = nullptr;
    pZesFrequencyGetProperties = nullptr;
    pZesFrequencyGetState = nullptr;
    pZesDeviceEnumTemperatureSensors = nullptr;
    pZesTemperatureGetProperties = nullptr;
    pZesTemperatureGetState = nullptr;
    pZesDeviceEnumPowerDomains = nullptr;
    pZesPowerGetProperties = nullptr;
    pZesPowerGetEnergyCounter = nullptr;
    pZeInit = nullptr;

    if (this->m_libHandle)
    {
        ::dlclose(this->m_libHandle);
        this->m_libHandle = nullptr;
    }
}
