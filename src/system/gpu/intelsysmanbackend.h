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

#ifndef GPUINTELSYSMANBACKEND_H
#define GPUINTELSYSMANBACKEND_H

#include "../gpu.h"

#include <QElapsedTimer>
#include <QHash>
#include <QStringList>
#include <memory>
#include <vector>

class GpuIntelSysmanBackend
{
    public:
        GpuIntelSysmanBackend() = default;
        ~GpuIntelSysmanBackend();

        void Detect();
        bool Sample(std::vector<std::unique_ptr<GPU::GPUInfo>> &gpus);
        bool IsAvailable() const { return this->m_available; }

    private:
        struct Snapshot
        {
            uint64_t value { 0 };
            uint64_t timestamp { 0 };
        };

        //! Busy counters for one engine class, either for a single client (a "this
        //! scan" reading, or the previous-scan baseline stored per client) or summed
        //! as a delta across clients — same shape, different granularity depending
        //! on where it is used.
        struct FdEngineSnapshot
        {
            quint64 busyNs { 0 };  ///< Legacy i915 "drm-engine-*" nanoseconds
            quint64 cycles { 0 };  ///< xe "drm-cycles-*" GPU clock-domain busy cycles
        };

        //! Per-engine-class metadata that is identical across every client sharing
        //! that class (unlike FdEngineSnapshot, which is per-client): xe's shared
        //! "drm-total-cycles-*" GT reference clock, and "drm-engine-capacity-*",
        //! the number of identical hardware instances summed into one counter.
        struct FdEngineClassState
        {
            quint64 prevTotalCycles { 0 };
            quint64 capacity { 1 };
        };

        //! Per-GPU state for the DRM fdinfo engine-activity fallback (used when
        //! sysman cannot enumerate engine groups, e.g. non-root processes on the
        //! xe driver), keyed by PCI BDF so multiple GPUs never share a render
        //! node, fd cache, or engine baseline.
        struct FdInfoGpuState
        {
            QString     renderNodePath;
            QString     cardNodePath;
            QStringList cachedPaths;
            int         rescanCounter { 0 };
            QHash<QString, FdEngineClassState> classes;           ///< engineKey -> class metadata
            QHash<QString, FdEngineSnapshot>   prevClientEngines; ///< "clientKey/engineKey" -> baseline
        };

        using FdPerClientEngines = QHash<QString /*clientKey*/, QHash<QString /*engineKey*/, FdEngineSnapshot>>;

        void unload();
        void sampleFdInfoEngines(GPU::GPUInfo &gpu, const QString &bdf, qint64 intervalNs);
        bool parseFdInfoFile(const QString &infoPath, const QString &pdevId,
                             QHash<QString, FdEngineSnapshot> &perEngine,
                             QHash<QString, quint64> &totalCyclesThisScan,
                             QHash<QString, quint64> &capacityThisScan,
                             int &clientId);
        FdPerClientEngines scanFdInfoEngines(FdInfoGpuState &state, const QString &pdevId,
                                              QHash<QString, quint64> &totalCyclesThisScan,
                                              QHash<QString, quint64> &capacityThisScan,
                                              bool fullRescan);
        void setFdEngineFallback(bool active);
        void setHwmonTempFallback(bool active);

        bool    m_available { false };
        void   *m_libHandle { nullptr };
        QHash<QString, Snapshot> m_prevEnergyById;
        QHash<QString, Snapshot> m_prevPciRxById;
        QHash<QString, Snapshot> m_prevPciTxById;
        QHash<QString, Snapshot> m_prevEngineByKey;

        // DRM fdinfo fallback state (used when sysman cannot enumerate engine groups,
        // e.g. non-root processes on the xe driver), one entry per PCI BDF.
        QHash<QString, FdInfoGpuState> m_fdInfoByBdf;
        QElapsedTimer m_fdInfoTimer;
        bool        m_fdInfoTimerStarted { false };
        bool        m_fdEngineFallbackActive { false };

        // hwmon temperature fallback state (used when sysman exposes no temp sensors,
        // e.g. systems without PMT telemetry nodes)
        bool        m_hwmonTempFallbackActive { false };
};

#endif // GPUINTELSYSMANBACKEND_H
