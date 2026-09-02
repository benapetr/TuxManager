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

        //! Aggregate per engine class from DRM fdinfo (drm-engine-* / drm-cycles-*).
        struct FdEngineSnapshot
        {
            quint64 busyNs { 0 };       ///< Legacy i915 nanosecond busy counters (summed over clients)
            quint64 cycles { 0 };       ///< xe drm-cycles-* busy cycles (summed over clients)
            quint64 totalCycles { 0 };  ///< xe drm-total-cycles-* GPU clock reference (shared by clients)
        };

    private:
        struct Snapshot
        {
            uint64_t value { 0 };
            uint64_t timestamp { 0 };
        };

        void unload();
        void sampleFdInfoEngines(GPU::GPUInfo &gpu, const QString &bdf, qint64 intervalNs);
        void setFdEngineFallback(bool active);
        void setHwmonTempFallback(bool active);

        bool    m_available { false };
        void   *m_libHandle { nullptr };
        QHash<QString, Snapshot> m_prevEnergyById;
        QHash<QString, Snapshot> m_prevPciRxById;
        QHash<QString, Snapshot> m_prevPciTxById;
        QHash<QString, Snapshot> m_prevEngineByKey;

        // DRM fdinfo fallback state (used when sysman cannot enumerate engine groups,
        // e.g. non-root processes on the xe driver)
        QHash<QString, FdEngineSnapshot> m_prevFdEngines;
        QStringList m_fdInfoPaths;
        QString     m_fdRenderNode;
        QElapsedTimer m_fdInfoTimer;
        bool        m_fdInfoTimerStarted { false };
        int         m_fdInfoRescanCounter { 0 };
        bool        m_fdEngineFallbackActive { false };

        // hwmon temperature fallback state (used when sysman exposes no temp sensors,
        // e.g. systems without PMT telemetry nodes)
        bool        m_hwmonTempFallbackActive { false };
};

#endif // GPUINTELSYSMANBACKEND_H
