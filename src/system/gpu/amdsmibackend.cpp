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

#include "amdsmibackend.h"
#include "../../logger.h"
#include "../../misc.h"

#include <QFileInfo>
#include <QSet>
#include <QSysInfo>
#include <dlfcn.h>

namespace
{
    using AmdSmiStatus = uint32_t;
    using AmdSmiSocketHandle = void *;
    using AmdSmiProcessorHandle = void *;

    static constexpr AmdSmiStatus AMDSMI_STATUS_SUCCESS = 0;
    static constexpr uint64_t AMDSMI_INIT_AMD_GPUS = (1ULL << 1);
    static constexpr unsigned int AMDSMI_GPU_UUID_SIZE = 38;
    static constexpr uint32_t AMDSMI_PROCESSOR_TYPE_AMD_GPU = 1;
    static constexpr uint32_t AMDSMI_CLK_TYPE_GFX = 1;
    static constexpr uint32_t AMDSMI_TEMPERATURE_TYPE_EDGE = 0;
    static constexpr uint32_t AMDSMI_TEMP_CURRENT = 0;

    union AmdSmiBdf
    {
        struct
        {
            uint64_t function_number : 3;
            uint64_t device_number : 5;
            uint64_t bus_number : 8;
            uint64_t domain_number : 48;
        } fields;
        uint64_t as_uint;
    };

    struct AmdSmiVramUsage
    {
        uint32_t vram_total;
        uint32_t vram_used;
        uint32_t reserved[2];
    };

    struct AmdSmiEngineUsage
    {
        uint32_t gfx_activity;
        uint32_t umc_activity;
        uint32_t mm_activity;
        uint32_t reserved[13];
    };

    struct AmdSmiPowerInfo
    {
        uint64_t socket_power;
        uint32_t current_socket_power;
        uint32_t average_socket_power;
        uint32_t gfx_voltage;
        uint32_t soc_voltage;
        uint32_t mem_voltage;
        uint32_t power_limit;
        uint32_t reserved[2];
    };

    struct AmdSmiClkInfo
    {
        uint64_t clk;
        uint64_t min_clk;
        uint64_t max_clk;
        bool clk_locked;
        bool clk_deep_sleep;
    };

    using FnAmdSmiInit = AmdSmiStatus (*)(uint64_t);
    using FnAmdSmiShutdown = AmdSmiStatus (*)(void);
    using FnAmdSmiGetSocketHandles = AmdSmiStatus (*)(uint32_t *, AmdSmiSocketHandle *);
    using FnAmdSmiGetProcessorHandlesByType = AmdSmiStatus (*)(AmdSmiSocketHandle, uint32_t, AmdSmiProcessorHandle *, uint32_t *);
    using FnAmdSmiGetProcessorInfo = AmdSmiStatus (*)(AmdSmiProcessorHandle, size_t, char *);
    using FnAmdSmiGetGpuDeviceBdf = AmdSmiStatus (*)(AmdSmiProcessorHandle, AmdSmiBdf *);
    using FnAmdSmiGetGpuDeviceUuid = AmdSmiStatus (*)(AmdSmiProcessorHandle, unsigned int *, char *);
    using FnAmdSmiGetGpuActivity = AmdSmiStatus (*)(AmdSmiProcessorHandle, AmdSmiEngineUsage *);
    using FnAmdSmiGetTempMetric = AmdSmiStatus (*)(AmdSmiProcessorHandle, uint32_t, uint32_t, int64_t *);
    using FnAmdSmiGetPowerInfo = AmdSmiStatus (*)(AmdSmiProcessorHandle, AmdSmiPowerInfo *);
    using FnAmdSmiGetClockInfo = AmdSmiStatus (*)(AmdSmiProcessorHandle, uint32_t, AmdSmiClkInfo *);
    using FnAmdSmiGetGpuVramUsage = AmdSmiStatus (*)(AmdSmiProcessorHandle, AmdSmiVramUsage *);

    FnAmdSmiInit pAmdSmiInit = nullptr;
    FnAmdSmiShutdown pAmdSmiShutdown = nullptr;
    FnAmdSmiGetSocketHandles pAmdSmiGetSocketHandles = nullptr;
    FnAmdSmiGetProcessorHandlesByType pAmdSmiGetProcessorHandlesByType = nullptr;
    FnAmdSmiGetProcessorInfo pAmdSmiGetProcessorInfo = nullptr;
    FnAmdSmiGetGpuDeviceBdf pAmdSmiGetGpuDeviceBdf = nullptr;
    FnAmdSmiGetGpuDeviceUuid pAmdSmiGetGpuDeviceUuid = nullptr;
    FnAmdSmiGetGpuActivity pAmdSmiGetGpuActivity = nullptr;
    FnAmdSmiGetTempMetric pAmdSmiGetTempMetric = nullptr;
    FnAmdSmiGetPowerInfo pAmdSmiGetPowerInfo = nullptr;
    FnAmdSmiGetClockInfo pAmdSmiGetClockInfo = nullptr;
    FnAmdSmiGetGpuVramUsage pAmdSmiGetGpuVramUsage = nullptr;

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

    QString bdfToString(const AmdSmiBdf &bdf)
    {
        return QStringLiteral("%1:%2:%3.%4")
            .arg(static_cast<qulonglong>(bdf.fields.domain_number), 4, 16, QLatin1Char('0'))
            .arg(static_cast<qulonglong>(bdf.fields.bus_number), 2, 16, QLatin1Char('0'))
            .arg(static_cast<qulonglong>(bdf.fields.device_number), 2, 16, QLatin1Char('0'))
            .arg(static_cast<qulonglong>(bdf.fields.function_number), 1, 16, QLatin1Char('0'));
    }

    QString detectDriverVersionFromBdf(const QString &bdf)
    {
        const QString devicePath = QStringLiteral("/sys/bus/pci/devices/") + bdf;
        QString driverName = Misc::FileNameFromSymlink(devicePath + QStringLiteral("/driver/module"));
        if (driverName.isEmpty())
            driverName = Misc::FileNameFromSymlink(devicePath + QStringLiteral("/driver"));
        if (!driverName.isEmpty())
        {
            const QString version = Misc::ReadFile(QStringLiteral("/sys/module/") + driverName + QStringLiteral("/version")).trimmed();
            if (!version.isEmpty())
                return version;
        }
        return QSysInfo::kernelVersion();
    }
}

GpuAmdSmiBackend::~GpuAmdSmiBackend()
{
    this->unload();
}

void GpuAmdSmiBackend::Detect()
{
    this->unload();

    this->m_libHandle = ::dlopen("libamd_smi.so", RTLD_LAZY | RTLD_LOCAL);
    if (!this->m_libHandle)
    {
        LOG_DEBUG("AMD SMI scan: unable to locate libamd_smi.so, trying fallback to libamd_smi.so.1");
        this->m_libHandle = ::dlopen("libamd_smi.so.1", RTLD_LAZY | RTLD_LOCAL);
    }

    if (!this->m_libHandle)
    {
        LOG_DEBUG("AMD SMI scan: no AMD SMI library detected");
        return;
    }

    pAmdSmiInit = reinterpret_cast<FnAmdSmiInit>(::dlsym(this->m_libHandle, "amdsmi_init"));
    pAmdSmiShutdown = reinterpret_cast<FnAmdSmiShutdown>(::dlsym(this->m_libHandle, "amdsmi_shut_down"));
    pAmdSmiGetSocketHandles = reinterpret_cast<FnAmdSmiGetSocketHandles>(::dlsym(this->m_libHandle, "amdsmi_get_socket_handles"));
    pAmdSmiGetProcessorHandlesByType = reinterpret_cast<FnAmdSmiGetProcessorHandlesByType>(::dlsym(this->m_libHandle, "amdsmi_get_processor_handles_by_type"));
    pAmdSmiGetProcessorInfo = reinterpret_cast<FnAmdSmiGetProcessorInfo>(::dlsym(this->m_libHandle, "amdsmi_get_processor_info"));
    pAmdSmiGetGpuDeviceBdf = reinterpret_cast<FnAmdSmiGetGpuDeviceBdf>(::dlsym(this->m_libHandle, "amdsmi_get_gpu_device_bdf"));
    pAmdSmiGetGpuDeviceUuid = reinterpret_cast<FnAmdSmiGetGpuDeviceUuid>(::dlsym(this->m_libHandle, "amdsmi_get_gpu_device_uuid"));
    pAmdSmiGetGpuActivity = reinterpret_cast<FnAmdSmiGetGpuActivity>(::dlsym(this->m_libHandle, "amdsmi_get_gpu_activity"));
    pAmdSmiGetTempMetric = reinterpret_cast<FnAmdSmiGetTempMetric>(::dlsym(this->m_libHandle, "amdsmi_get_temp_metric"));
    pAmdSmiGetPowerInfo = reinterpret_cast<FnAmdSmiGetPowerInfo>(::dlsym(this->m_libHandle, "amdsmi_get_power_info"));
    pAmdSmiGetClockInfo = reinterpret_cast<FnAmdSmiGetClockInfo>(::dlsym(this->m_libHandle, "amdsmi_get_clock_info"));
    pAmdSmiGetGpuVramUsage = reinterpret_cast<FnAmdSmiGetGpuVramUsage>(::dlsym(this->m_libHandle, "amdsmi_get_gpu_vram_usage"));

    const bool symbolsOk = pAmdSmiInit && pAmdSmiShutdown && pAmdSmiGetSocketHandles
                           && pAmdSmiGetProcessorHandlesByType && pAmdSmiGetGpuDeviceBdf
                           && pAmdSmiGetGpuActivity && pAmdSmiGetPowerInfo
                           && pAmdSmiGetClockInfo && pAmdSmiGetGpuVramUsage;
    if (!symbolsOk)
    {
        LOG_DEBUG("AMD SMI: required symbols not found, unloading");
        this->unload();
        return;
    }

    if (pAmdSmiInit(AMDSMI_INIT_AMD_GPUS) != AMDSMI_STATUS_SUCCESS)
    {
        LOG_DEBUG("AMD SMI: init failed, unloading");
        this->unload();
        return;
    }

    uint32_t socketCount = 0;
    if (pAmdSmiGetSocketHandles(&socketCount, nullptr) != AMDSMI_STATUS_SUCCESS || socketCount == 0)
    {
        LOG_DEBUG("AMD SMI: no sockets found, shutting down");
        this->unload();
        return;
    }

    std::vector<AmdSmiSocketHandle> sockets(socketCount);
    if (pAmdSmiGetSocketHandles(&socketCount, sockets.data()) != AMDSMI_STATUS_SUCCESS)
    {
        LOG_DEBUG("AMD SMI: failed to enumerate sockets, shutting down");
        this->unload();
        return;
    }

    bool hasGpu = false;
    for (AmdSmiSocketHandle socket : sockets)
    {
        uint32_t gpuCount = 0;
        if (pAmdSmiGetProcessorHandlesByType(socket, AMDSMI_PROCESSOR_TYPE_AMD_GPU, nullptr, &gpuCount) == AMDSMI_STATUS_SUCCESS
            && gpuCount > 0)
        {
            hasGpu = true;
            break;
        }
    }

    if (!hasGpu)
    {
        LOG_DEBUG("AMD SMI: no AMD GPUs found, shutting down");
        this->unload();
        return;
    }

    this->m_available = true;
}

bool GpuAmdSmiBackend::Sample(std::vector<std::unique_ptr<GPU::GPUInfo>> &gpus)
{
    if (!this->m_available)
        return false;

    uint32_t socketCount = 0;
    if (pAmdSmiGetSocketHandles(&socketCount, nullptr) != AMDSMI_STATUS_SUCCESS || socketCount == 0)
        return false;

    std::vector<AmdSmiSocketHandle> sockets(socketCount);
    if (pAmdSmiGetSocketHandles(&socketCount, sockets.data()) != AMDSMI_STATUS_SUCCESS)
        return false;

    QSet<QString> seenIds;

    for (AmdSmiSocketHandle socket : sockets)
    {
        uint32_t gpuCount = 0;
        if (pAmdSmiGetProcessorHandlesByType(socket, AMDSMI_PROCESSOR_TYPE_AMD_GPU, nullptr, &gpuCount) != AMDSMI_STATUS_SUCCESS
            || gpuCount == 0)
            continue;

        std::vector<AmdSmiProcessorHandle> processors(gpuCount);
        if (pAmdSmiGetProcessorHandlesByType(socket, AMDSMI_PROCESSOR_TYPE_AMD_GPU, processors.data(), &gpuCount) != AMDSMI_STATUS_SUCCESS)
            continue;

        for (uint32_t i = 0; i < gpuCount; ++i)
        {
            AmdSmiProcessorHandle processor = processors[i];

            AmdSmiBdf bdf{};
            if (pAmdSmiGetGpuDeviceBdf(processor, &bdf) != AMDSMI_STATUS_SUCCESS)
                continue;

            const QString bdfString = bdfToString(bdf);

            QString id = bdfString;
            if (pAmdSmiGetGpuDeviceUuid)
            {
                char uuidBuf[AMDSMI_GPU_UUID_SIZE + 1] = {};
                unsigned int uuidLen = AMDSMI_GPU_UUID_SIZE + 1;
                if (pAmdSmiGetGpuDeviceUuid(processor, &uuidLen, uuidBuf) == AMDSMI_STATUS_SUCCESS)
                {
                    const QString uuid = QString::fromLatin1(uuidBuf).trimmed();
                    if (!uuid.isEmpty())
                        id = uuid;
                }
            }

            char nameBuf[128] = {};
            QString name = QStringLiteral("AMD Radeon");
            if (pAmdSmiGetProcessorInfo && pAmdSmiGetProcessorInfo(processor, sizeof(nameBuf), nameBuf) == AMDSMI_STATUS_SUCCESS)
            {
                const QString rawName = QString::fromLatin1(nameBuf).trimmed();
                if (!rawName.isEmpty())
                    name = rawName;
            }

            AmdSmiEngineUsage activity{};
            const bool hasActivity = pAmdSmiGetGpuActivity(processor, &activity) == AMDSMI_STATUS_SUCCESS;

            int64_t temperatureMilliC = 0;
            const bool hasTemp = pAmdSmiGetTempMetric
                                 && pAmdSmiGetTempMetric(processor, AMDSMI_TEMPERATURE_TYPE_EDGE, AMDSMI_TEMP_CURRENT, &temperatureMilliC) == AMDSMI_STATUS_SUCCESS;

            AmdSmiPowerInfo powerInfo{};
            const bool hasPower = pAmdSmiGetPowerInfo(processor, &powerInfo) == AMDSMI_STATUS_SUCCESS;

            AmdSmiClkInfo clkInfo{};
            const bool hasClock = pAmdSmiGetClockInfo(processor, AMDSMI_CLK_TYPE_GFX, &clkInfo) == AMDSMI_STATUS_SUCCESS;

            AmdSmiVramUsage vram{};
            const bool hasVram = pAmdSmiGetGpuVramUsage(processor, &vram) == AMDSMI_STATUS_SUCCESS;

            GPU::GPUInfo &gpu = *findOrCreateGpu(gpus, id);
            gpu.ID = id;
            gpu.Name = name;
            gpu.DriverVersion = detectDriverVersionFromBdf(bdfString);
            gpu.Backend = QStringLiteral("AMD SMI");
            gpu.UtilPct = hasActivity ? qBound(0.0, static_cast<double>(activity.gfx_activity), 100.0) : 0.0;
            gpu.TemperatureC = hasTemp ? static_cast<int>(temperatureMilliC / 1000) : -1;
            gpu.CoreClockMHz = hasClock ? static_cast<int>(clkInfo.clk) : -1;
            gpu.PowerUsageW = hasPower ? static_cast<double>(powerInfo.average_socket_power) : -1.0;
            gpu.MemUsedMiB = hasVram ? static_cast<qint64>(vram.vram_used) : 0;
            gpu.MemTotalMiB = hasVram ? static_cast<qint64>(vram.vram_total) : 0;
            gpu.SharedMemUsedMiB = 0;
            gpu.SharedMemTotalMiB = 0;
            gpu.CopyTxBps = 0.0;
            gpu.CopyRxBps = 0.0;
            gpu.UtilHistory.Push(gpu.UtilPct);
            const double memPct = gpu.MemTotalMiB > 0
                                      ? static_cast<double>(gpu.MemUsedMiB) / static_cast<double>(gpu.MemTotalMiB) * 100.0
                                      : 0.0;
            gpu.MemUsageHistory.Push(memPct);
            gpu.SharedMemHistory.Push(0.0);
            Misc::PushHistoryAndUpdateMax(gpu.CopyTxHistory, 0.0, gpu.MaxCopyBps);
            Misc::PushHistoryAndUpdateMax(gpu.CopyRxHistory, 0.0, gpu.MaxCopyBps);

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

            if (hasActivity)
            {
                addEngine(QStringLiteral("gfx"), QStringLiteral("GFX"), activity.gfx_activity);
                addEngine(QStringLiteral("mem"), QStringLiteral("MEM"), activity.umc_activity);
                addEngine(QStringLiteral("media"), QStringLiteral("媒体"), activity.mm_activity);
            }

            zeroMissingEngines(gpu, seenEngineKeys);
            seenIds.insert(id);
        }
    }

    for (const auto &gpu : gpus)
    {
        if (gpu->Backend != QLatin1String("AMD SMI") || seenIds.contains(gpu->ID))
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

    return !seenIds.isEmpty();
}

void GpuAmdSmiBackend::unload()
{
    if (this->m_available && pAmdSmiShutdown)
        pAmdSmiShutdown();

    this->m_available = false;

    pAmdSmiInit = nullptr;
    pAmdSmiShutdown = nullptr;
    pAmdSmiGetSocketHandles = nullptr;
    pAmdSmiGetProcessorHandlesByType = nullptr;
    pAmdSmiGetProcessorInfo = nullptr;
    pAmdSmiGetGpuDeviceBdf = nullptr;
    pAmdSmiGetGpuDeviceUuid = nullptr;
    pAmdSmiGetGpuActivity = nullptr;
    pAmdSmiGetTempMetric = nullptr;
    pAmdSmiGetPowerInfo = nullptr;
    pAmdSmiGetClockInfo = nullptr;
    pAmdSmiGetGpuVramUsage = nullptr;

    if (this->m_libHandle)
    {
        ::dlclose(this->m_libHandle);
        this->m_libHandle = nullptr;
    }
}
