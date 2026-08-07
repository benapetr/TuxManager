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

#include "nvmlbackend.h"
#include "../../logger.h"
#include "../../misc.h"

#include <QSet>
#include <dlfcn.h>

namespace
{
    using NvmlReturn = unsigned int;
    using NvmlDevice = void *;

    struct NvmlUtilization
    {
        unsigned int gpu;
        unsigned int memory;
    };

    struct NvmlMemory
    {
        uint64_t total;
        uint64_t free;
        uint64_t used;
    };

    struct NvmlProcessInfo_v2
    {
        unsigned int pid;
        uint64_t     usedGpuMemory;
        unsigned int gpuInstanceId;
        unsigned int computeInstanceId;
    };

    struct NvmlProcessUtilizationSample
    {
        unsigned int pid;
        uint64_t     timeStamp;
        unsigned int smUtil;
        unsigned int memUtil;
        unsigned int encUtil;
        unsigned int decUtil;
    };

    static constexpr NvmlReturn NVML_SUCCESS = 0;

    using FnNvmlInitV2 = NvmlReturn (*)();
    using FnNvmlShutdown = NvmlReturn (*)();
    using FnNvmlSystemGetDriverVersion = NvmlReturn (*)(char *, unsigned int);
    using FnNvmlDeviceGetCountV2 = NvmlReturn (*)(unsigned int *);
    using FnNvmlDeviceGetHandleByIndexV2 = NvmlReturn (*)(unsigned int, NvmlDevice *);
    using FnNvmlDeviceGetName = NvmlReturn (*)(NvmlDevice, char *, unsigned int);
    using FnNvmlDeviceGetUUID = NvmlReturn (*)(NvmlDevice, char *, unsigned int);
    using FnNvmlDeviceGetUtilizationRates = NvmlReturn (*)(NvmlDevice, NvmlUtilization *);
    using FnNvmlDeviceGetMemoryInfo = NvmlReturn (*)(NvmlDevice, NvmlMemory *);
    using FnNvmlDeviceGetEncoderUtilization = NvmlReturn (*)(NvmlDevice, unsigned int *, unsigned int *);
    using FnNvmlDeviceGetDecoderUtilization = NvmlReturn (*)(NvmlDevice, unsigned int *, unsigned int *);
    using FnNvmlDeviceGetPcieThroughput = NvmlReturn (*)(NvmlDevice, unsigned int, unsigned int *);
    using FnNvmlDeviceGetTemperature = NvmlReturn (*)(NvmlDevice, unsigned int, unsigned int *);
    using FnNvmlDeviceGetClockInfo = NvmlReturn (*)(NvmlDevice, unsigned int, unsigned int *);
    using FnNvmlDeviceGetPowerUsage = NvmlReturn (*)(NvmlDevice, unsigned int *);
    using FnNvmlDeviceGetGraphicsRunningProcesses = NvmlReturn (*)(NvmlDevice, unsigned int *, NvmlProcessInfo_v2 *);
    using FnNvmlDeviceGetComputeRunningProcesses = NvmlReturn (*)(NvmlDevice, unsigned int *, NvmlProcessInfo_v2 *);
    using FnNvmlDeviceGetProcessUtilization = NvmlReturn (*)(NvmlDevice, NvmlProcessUtilizationSample *, unsigned int *, uint64_t);

    FnNvmlDeviceGetGraphicsRunningProcesses pNvmlDeviceGetGraphicsRunningProcesses = nullptr;
    FnNvmlDeviceGetComputeRunningProcesses pNvmlDeviceGetComputeRunningProcesses = nullptr;
    FnNvmlDeviceGetProcessUtilization pNvmlDeviceGetProcessUtilization = nullptr;

    FnNvmlInitV2 pNvmlInitV2 = nullptr;
    FnNvmlShutdown pNvmlShutdown = nullptr;
    FnNvmlSystemGetDriverVersion pNvmlSystemGetDriverVersion = nullptr;
    FnNvmlDeviceGetCountV2 pNvmlDeviceGetCountV2 = nullptr;
    FnNvmlDeviceGetHandleByIndexV2 pNvmlDeviceGetHandleByIndexV2 = nullptr;
    FnNvmlDeviceGetName pNvmlDeviceGetName = nullptr;
    FnNvmlDeviceGetUUID pNvmlDeviceGetUUID = nullptr;
    FnNvmlDeviceGetUtilizationRates pNvmlDeviceGetUtilizationRates = nullptr;
    FnNvmlDeviceGetMemoryInfo pNvmlDeviceGetMemoryInfo = nullptr;
    FnNvmlDeviceGetEncoderUtilization pNvmlDeviceGetEncoderUtilization = nullptr;
    FnNvmlDeviceGetDecoderUtilization pNvmlDeviceGetDecoderUtilization = nullptr;
    FnNvmlDeviceGetPcieThroughput pNvmlDeviceGetPcieThroughput = nullptr;
    FnNvmlDeviceGetTemperature pNvmlDeviceGetTemperature = nullptr;
    FnNvmlDeviceGetClockInfo pNvmlDeviceGetClockInfo = nullptr;
    FnNvmlDeviceGetPowerUsage pNvmlDeviceGetPowerUsage = nullptr;

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

GpuNvmlBackend::~GpuNvmlBackend()
{
    this->unload();
}

void GpuNvmlBackend::Detect()
{
    this->unload();

    this->m_libHandle = ::dlopen("libnvidia-ml.so.1", RTLD_LAZY | RTLD_LOCAL);
    if (!this->m_libHandle)
    {
        LOG_DEBUG("NVML scan: unable to locate libnvidia-ml.so.1, trying fallback to libnvidia-ml.so");
        this->m_libHandle = ::dlopen("libnvidia-ml.so", RTLD_LAZY | RTLD_LOCAL);
    }

    if (!this->m_libHandle)
    {
        LOG_DEBUG("NVML scan: no nvidia driver libs detected");
        return;
    }

    pNvmlInitV2 = reinterpret_cast<FnNvmlInitV2>(::dlsym(this->m_libHandle, "nvmlInit_v2"));
    pNvmlShutdown = reinterpret_cast<FnNvmlShutdown>(::dlsym(this->m_libHandle, "nvmlShutdown"));
    pNvmlSystemGetDriverVersion = reinterpret_cast<FnNvmlSystemGetDriverVersion>(::dlsym(this->m_libHandle, "nvmlSystemGetDriverVersion"));
    pNvmlDeviceGetCountV2 = reinterpret_cast<FnNvmlDeviceGetCountV2>(::dlsym(this->m_libHandle, "nvmlDeviceGetCount_v2"));
    pNvmlDeviceGetHandleByIndexV2 = reinterpret_cast<FnNvmlDeviceGetHandleByIndexV2>(::dlsym(this->m_libHandle, "nvmlDeviceGetHandleByIndex_v2"));
    pNvmlDeviceGetName = reinterpret_cast<FnNvmlDeviceGetName>(::dlsym(this->m_libHandle, "nvmlDeviceGetName"));
    pNvmlDeviceGetUUID = reinterpret_cast<FnNvmlDeviceGetUUID>(::dlsym(this->m_libHandle, "nvmlDeviceGetUUID"));
    pNvmlDeviceGetUtilizationRates = reinterpret_cast<FnNvmlDeviceGetUtilizationRates>(::dlsym(this->m_libHandle, "nvmlDeviceGetUtilizationRates"));
    pNvmlDeviceGetMemoryInfo = reinterpret_cast<FnNvmlDeviceGetMemoryInfo>(::dlsym(this->m_libHandle, "nvmlDeviceGetMemoryInfo"));
    pNvmlDeviceGetEncoderUtilization = reinterpret_cast<FnNvmlDeviceGetEncoderUtilization>(::dlsym(this->m_libHandle, "nvmlDeviceGetEncoderUtilization"));
    pNvmlDeviceGetDecoderUtilization = reinterpret_cast<FnNvmlDeviceGetDecoderUtilization>(::dlsym(this->m_libHandle, "nvmlDeviceGetDecoderUtilization"));
    pNvmlDeviceGetPcieThroughput = reinterpret_cast<FnNvmlDeviceGetPcieThroughput>(::dlsym(this->m_libHandle, "nvmlDeviceGetPcieThroughput"));
    pNvmlDeviceGetTemperature = reinterpret_cast<FnNvmlDeviceGetTemperature>(::dlsym(this->m_libHandle, "nvmlDeviceGetTemperature"));
    pNvmlDeviceGetClockInfo = reinterpret_cast<FnNvmlDeviceGetClockInfo>(::dlsym(this->m_libHandle, "nvmlDeviceGetClockInfo"));
    pNvmlDeviceGetPowerUsage = reinterpret_cast<FnNvmlDeviceGetPowerUsage>(::dlsym(this->m_libHandle, "nvmlDeviceGetPowerUsage"));
    pNvmlDeviceGetGraphicsRunningProcesses = reinterpret_cast<FnNvmlDeviceGetGraphicsRunningProcesses>(::dlsym(this->m_libHandle, "nvmlDeviceGetGraphicsRunningProcesses_v3"));
    pNvmlDeviceGetComputeRunningProcesses = reinterpret_cast<FnNvmlDeviceGetComputeRunningProcesses>(::dlsym(this->m_libHandle, "nvmlDeviceGetComputeRunningProcesses_v3"));
    pNvmlDeviceGetProcessUtilization = reinterpret_cast<FnNvmlDeviceGetProcessUtilization>(::dlsym(this->m_libHandle, "nvmlDeviceGetProcessUtilization"));

    const bool symbolsOk = pNvmlInitV2 && pNvmlShutdown && pNvmlDeviceGetCountV2
                           && pNvmlDeviceGetHandleByIndexV2 && pNvmlDeviceGetName
                           && pNvmlDeviceGetUUID && pNvmlDeviceGetUtilizationRates
                           && pNvmlDeviceGetMemoryInfo;
    if (!symbolsOk)
    {
        LOG_DEBUG("NVML: required symbols not found, unloading");
        this->unload();
        return;
    }

    if (pNvmlInitV2() != NVML_SUCCESS)
    {
        LOG_DEBUG("NVML: nvmlInit_v2 failed, unloading");
        this->unload();
        return;
    }

    unsigned int count = 0;
    if (pNvmlDeviceGetCountV2(&count) != NVML_SUCCESS || count == 0)
    {
        LOG_DEBUG("NVML: no devices found, shutting down");
        this->unload();
        return;
    }

    this->m_available = true;
}

bool GpuNvmlBackend::Sample(std::vector<std::unique_ptr<GPU::GPUInfo>> &gpus)
{
    if (!this->m_available || !pNvmlDeviceGetCountV2 || !pNvmlDeviceGetHandleByIndexV2)
        return false;

    unsigned int count = 0;
    if (pNvmlDeviceGetCountV2(&count) != NVML_SUCCESS)
        return false;

    char driverVer[96] = {};
    if (pNvmlSystemGetDriverVersion)
        pNvmlSystemGetDriverVersion(driverVer, sizeof(driverVer));
    const QString driverVersion = QString::fromLatin1(driverVer).trimmed();

    QSet<QString> seenIds;

    for (unsigned int i = 0; i < count; ++i)
    {
        static constexpr unsigned int kNvmlPcieTxBytes = 0;
        static constexpr unsigned int kNvmlPcieRxBytes = 1;
        static constexpr unsigned int kNvmlTemperatureGpu = 0;
        static constexpr unsigned int kNvmlClockGraphics = 0;

        NvmlDevice dev = nullptr;
        if (pNvmlDeviceGetHandleByIndexV2(i, &dev) != NVML_SUCCESS || !dev)
            continue;

        char nameBuf[128] = {};
        char uuidBuf[96] = {};
        pNvmlDeviceGetName(dev, nameBuf, sizeof(nameBuf));
        pNvmlDeviceGetUUID(dev, uuidBuf, sizeof(uuidBuf));

        const QString name = QString::fromLatin1(nameBuf).trimmed();
        QString id = QString::fromLatin1(uuidBuf).trimmed();
        if (id.isEmpty())
            id = QString("gpu-%1").arg(i);

        NvmlUtilization util{};
        NvmlMemory mem{};
        const bool hasUtil = pNvmlDeviceGetUtilizationRates(dev, &util) == NVML_SUCCESS;
        const bool hasMem = pNvmlDeviceGetMemoryInfo(dev, &mem) == NVML_SUCCESS;
        unsigned int txKBps = 0;
        unsigned int rxKBps = 0;
        const bool hasTx = pNvmlDeviceGetPcieThroughput
                           && pNvmlDeviceGetPcieThroughput(dev, kNvmlPcieTxBytes, &txKBps) == NVML_SUCCESS;
        const bool hasRx = pNvmlDeviceGetPcieThroughput
                           && pNvmlDeviceGetPcieThroughput(dev, kNvmlPcieRxBytes, &rxKBps) == NVML_SUCCESS;
        unsigned int tempC = 0;
        const bool hasTemp = pNvmlDeviceGetTemperature
                             && pNvmlDeviceGetTemperature(dev, kNvmlTemperatureGpu, &tempC) == NVML_SUCCESS;
        unsigned int coreClockMHz = 0;
        const bool hasCoreClock = pNvmlDeviceGetClockInfo
                                  && pNvmlDeviceGetClockInfo(dev, kNvmlClockGraphics, &coreClockMHz) == NVML_SUCCESS;
        unsigned int powerMilliW = 0;
        const bool hasPower = pNvmlDeviceGetPowerUsage
                              && pNvmlDeviceGetPowerUsage(dev, &powerMilliW) == NVML_SUCCESS;

        GPU::GPUInfo &gpu = *findOrCreateGpu(gpus, id);
        gpu.ID = id;
        gpu.Name = name;
        gpu.DriverVersion = driverVersion;
        gpu.Backend = QStringLiteral("NVML");
        gpu.UtilPct = hasUtil ? qBound(0.0, static_cast<double>(util.gpu), 100.0) : 0.0;
        gpu.TemperatureC = hasTemp ? static_cast<int>(tempC) : -1;
        gpu.CoreClockMHz = hasCoreClock ? static_cast<int>(coreClockMHz) : -1;
        gpu.PowerUsageW = hasPower ? static_cast<double>(powerMilliW) / 1000.0 : -1.0;
        gpu.MemUsedMiB = hasMem ? static_cast<qint64>(mem.used / (1024ULL * 1024ULL)) : 0;
        gpu.MemTotalMiB = hasMem ? static_cast<qint64>(mem.total / (1024ULL * 1024ULL)) : 0;
        gpu.SharedMemUsedMiB = 0;
        gpu.SharedMemTotalMiB = 0;
        gpu.CopyTxBps = hasTx ? static_cast<double>(txKBps) * 1024.0 : 0.0;
        gpu.CopyRxBps = hasRx ? static_cast<double>(rxKBps) * 1024.0 : 0.0;
        gpu.UtilHistory.Push(gpu.UtilPct);
        const double memPct = gpu.MemTotalMiB > 0
                                  ? static_cast<double>(gpu.MemUsedMiB) / static_cast<double>(gpu.MemTotalMiB) * 100.0
                                  : 0.0;
        gpu.MemUsageHistory.Push(memPct);
        gpu.SharedMemHistory.Push(0.0);
        Misc::PushHistoryAndUpdateMax(gpu.CopyTxHistory, gpu.CopyTxBps, gpu.MaxCopyBps);
        Misc::PushHistoryAndUpdateMax(gpu.CopyRxHistory, gpu.CopyRxBps, gpu.MaxCopyBps);

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

        if (hasUtil)
        {
            double graphicsUtil = util.gpu;
            double computeUtil = util.gpu;

            if (pNvmlDeviceGetGraphicsRunningProcesses
                && pNvmlDeviceGetComputeRunningProcesses
                && pNvmlDeviceGetProcessUtilization)
            {
                auto collectPids = [&dev](auto fn) -> QSet<unsigned int>
                {
                    unsigned int count = 0;
                    fn(dev, &count, nullptr);
                    QSet<unsigned int> pids;
                    if (count > 0)
                    {
                        QVector<NvmlProcessInfo_v2> procs(static_cast<int>(count));
                        if (fn(dev, &count, procs.data()) == NVML_SUCCESS)
                        {
                            for (unsigned int p = 0; p < count; ++p)
                                pids.insert(procs[static_cast<int>(p)].pid);
                        }
                    }
                    return pids;
                };

                const QSet<unsigned int> gfxPids = collectPids(pNvmlDeviceGetGraphicsRunningProcesses);
                const QSet<unsigned int> compPids = collectPids(pNvmlDeviceGetComputeRunningProcesses);

                unsigned int procUtilCount = 32;
                QVector<NvmlProcessUtilizationSample> procUtil(static_cast<int>(procUtilCount));
                NvmlReturn ret = pNvmlDeviceGetProcessUtilization(dev, procUtil.data(), &procUtilCount, 0);
                if (ret != NVML_SUCCESS && procUtilCount > 32 && procUtilCount <= 1024)
                {
                    procUtil.resize(static_cast<int>(procUtilCount));
                    ret = pNvmlDeviceGetProcessUtilization(dev, procUtil.data(), &procUtilCount, 0);
                }
                if (ret == NVML_SUCCESS)
                {
                    double sumGfx = 0.0;
                    double sumComp = 0.0;
                    for (unsigned int p = 0; p < procUtilCount; ++p)
                    {
                        const auto &sample = procUtil[static_cast<int>(p)];
                        if (compPids.contains(sample.pid) && !gfxPids.contains(sample.pid))
                            sumComp += sample.smUtil;
                        else
                            sumGfx += sample.smUtil;
                    }
                    graphicsUtil = qBound(0.0, sumGfx, 100.0);
                    computeUtil = qBound(0.0, sumComp, 100.0);
                }
            }

            addEngine(QStringLiteral("3d"), QStringLiteral("3D"), graphicsUtil);
            addEngine(QStringLiteral("cuda"), QStringLiteral("CUDA"), computeUtil);
            addEngine(QStringLiteral("copy"), QStringLiteral("复制"), util.memory);
        }
        if (pNvmlDeviceGetEncoderUtilization)
        {
            unsigned int enc = 0;
            unsigned int sampling = 0;
            if (pNvmlDeviceGetEncoderUtilization(dev, &enc, &sampling) == NVML_SUCCESS)
                addEngine(QStringLiteral("video-encode"), QStringLiteral("视频编码"), enc);
        }
        if (pNvmlDeviceGetDecoderUtilization)
        {
            unsigned int dec = 0;
            unsigned int sampling = 0;
            if (pNvmlDeviceGetDecoderUtilization(dev, &dec, &sampling) == NVML_SUCCESS)
                addEngine(QStringLiteral("video-decode"), QStringLiteral("视频解码"), dec);
        }

        zeroMissingEngines(gpu, seenEngineKeys);
        seenIds.insert(id);
    }

    for (const auto &gpu : gpus)
    {
        if (gpu->Backend != QLatin1String("NVML") || seenIds.contains(gpu->ID))
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

    return true;
}

void GpuNvmlBackend::unload()
{
    if (this->m_available && pNvmlShutdown)
        pNvmlShutdown();

    this->m_available = false;

    pNvmlInitV2 = nullptr;
    pNvmlShutdown = nullptr;
    pNvmlSystemGetDriverVersion = nullptr;
    pNvmlDeviceGetCountV2 = nullptr;
    pNvmlDeviceGetHandleByIndexV2 = nullptr;
    pNvmlDeviceGetName = nullptr;
    pNvmlDeviceGetUUID = nullptr;
    pNvmlDeviceGetUtilizationRates = nullptr;
    pNvmlDeviceGetMemoryInfo = nullptr;
    pNvmlDeviceGetEncoderUtilization = nullptr;
    pNvmlDeviceGetDecoderUtilization = nullptr;
    pNvmlDeviceGetPcieThroughput = nullptr;
    pNvmlDeviceGetTemperature = nullptr;
    pNvmlDeviceGetClockInfo = nullptr;
    pNvmlDeviceGetPowerUsage = nullptr;
    pNvmlDeviceGetGraphicsRunningProcesses = nullptr;
    pNvmlDeviceGetComputeRunningProcesses = nullptr;
    pNvmlDeviceGetProcessUtilization = nullptr;

    if (this->m_libHandle)
    {
        ::dlclose(this->m_libHandle);
        this->m_libHandle = nullptr;
    }
}
