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

#ifndef PERF_PERFDATAPROVIDER_H
#define PERF_PERFDATAPROVIDER_H

#include "historybuffer.h"

#include <QObject>
#include <QTimer>
#include <QVector>
#include <QString>
#include <QElapsedTimer>
#include <QSet>
#include <QHash>

namespace Perf
{
    /// Number of historical samples kept per metric (max graph window: 15 min at 1 Hz).
    static constexpr int HISTORY_SIZE = 900;

    /// Periodically samples /proc/stat (CPU) and /proc/meminfo (memory).
    /// All widgets that display performance data connect to the updated() signal
    /// and read values through the const accessors.
    class PerfDataProvider : public QObject
    {
        Q_OBJECT

        public:
            explicit PerfDataProvider(QObject *parent = nullptr);
            ~PerfDataProvider() override;

            void SetInterval(int ms);
            void SetActive(bool active);
            bool IsActive() const { return this->m_active; }
            void SetProcessStatsEnabled(bool enabled) { this->m_processStatsEnabled = enabled; }
            void SetCpuSamplingEnabled(bool enabled) { this->m_cpuSamplingEnabled = enabled; }
            void SetMemorySamplingEnabled(bool enabled) { this->m_memorySamplingEnabled = enabled; }
            void SetSwapSamplingEnabled(bool enabled) { this->m_swapSamplingEnabled = enabled; }
            void SetDiskSamplingEnabled(bool enabled) { this->m_diskSamplingEnabled = enabled; }
            void SetNetworkSamplingEnabled(bool enabled) { this->m_networkSamplingEnabled = enabled; }
            void SetGpuSamplingEnabled(bool enabled) { this->m_gpuSamplingEnabled = enabled; }

            // ── Aggregate CPU ─────────────────────────────────────────────────────
            double CpuPercent()  const { return this->m_cpuHistory.isEmpty() ? 0.0 : this->m_cpuHistory.last(); }
            const HistoryBuffer &CpuHistory()       const { return this->m_cpuHistory; }
            const HistoryBuffer &CpuKernelHistory() const { return this->m_cpuKernelHistory; }

            // ── Per-core CPU ──────────────────────────────────────────────────────
            int    CoreCount()                          const { return this->m_cores.size(); }
            double CorePercent(int i)                   const;
            const  HistoryBuffer &CoreHistory(int i)       const;
            const  HistoryBuffer &CoreKernelHistory(int i) const;

            // ── CPU metadata (read once at startup) ───────────────────────────────
            const QString &CpuModelName() const { return this->m_cpuModelName; }
            double CpuBaseMhz()           const { return this->m_cpuBaseMhz;   }
            double CpuCurrentMhz()        const { return this->m_cpuCurrentMhz; }
            double CoreCurrentMhz(int i)  const;
            int    CpuLogicalCount()      const { return this->m_cpuLogicalCount; }
            bool   CpuIsVirtualMachine()  const { return this->m_cpuIsVirtualMachine; }
            const QString &CpuVmVendor()  const { return this->m_cpuVmVendor; }
            int    CpuTemperatureC()      const { return this->m_cpuTemperatureC; }

            // ── Process / thread counts (updated every sample) ────────────────────
            int ProcessCount() const { return this->m_processCount; }
            int ThreadCount()  const { return this->m_threadCount;  }

            // ── Memory ───────────────────────────────────────────────────────────
            qint64 MemTotalKb()   const { return this->m_memTotalKb;   }
            /// In-use (htop formula): Total - Free - Buffers - PageCache
            qint64 MemUsedKb()    const { return this->m_memUsedKb;    }
            qint64 MemAvailKb()   const { return this->m_memAvailKb;   }
            /// Truly free (MemFree from /proc/meminfo)
            qint64 MemFreeKb()    const { return this->m_memFreeKb;    }
            /// Page cache: Cached + SReclaimable - Shmem + Buffers
            qint64 MemCachedKb()  const { return this->m_memCachedKb;  }
            qint64 MemBuffersKb() const { return this->m_memBuffersKb; }
            /// Dirty pages pending write-back: Dirty + Writeback
            qint64 MemDirtyKb()   const { return this->m_memDirtyKb;   }
            int MemDimmSlotsTotal() const { return this->m_memDimmSlotsTotal; }
            int MemDimmSlotsUsed()  const { return this->m_memDimmSlotsUsed;  }
            int MemSpeedMtps()      const { return this->m_memSpeedMtps;      }
            double MemFraction()  const;
            const HistoryBuffer &MemHistory() const { return this->m_memHistory; }

            // ── Swap ─────────────────────────────────────────────────────────────
            qint64 SwapTotalKb() const { return this->m_swapTotalKb; }
            qint64 SwapUsedKb()  const { return this->m_swapUsedKb;  }
            qint64 SwapFreeKb()  const { return this->m_swapFreeKb;  }
            double SwapInBytesPerSec()  const { return this->m_swapInBps;  }
            double SwapOutBytesPerSec() const { return this->m_swapOutBps; }
            double SwapMaxActivityBytesPerSec() const { return this->m_swapMaxActivityBps; }
            const HistoryBuffer &SwapUsageHistory() const { return this->m_swapUsageHistory; }
            const HistoryBuffer &SwapInHistory()    const { return this->m_swapInHistory;    }
            const HistoryBuffer &SwapOutHistory()   const { return this->m_swapOutHistory;   }

            // ── Disks (physical / virtual block devices from sysfs, excluding pseudo devices) ──
            int DiskCount() const { return this->m_disks.size(); }
            QString DiskName(int i) const;
            QString DiskModel(int i) const;
            QString DiskType(int i) const;
            double DiskActivePercent(int i) const;
            double DiskReadBytesPerSec(int i) const;
            double DiskWriteBytesPerSec(int i) const;
            double DiskMaxTransferBytesPerSec(int i) const;
            qint64 DiskCapacityBytes(int i) const;
            qint64 DiskFormattedBytes(int i) const;
            bool DiskIsSystemDisk(int i) const;
            bool DiskHasSwapFile(int i) const;
            const HistoryBuffer &DiskActiveHistory(int i) const;
            const HistoryBuffer &DiskReadHistory(int i) const;
            const HistoryBuffer &DiskWriteHistory(int i) const;

            // ── Network interfaces (active, non-loopback) ──────────────────────
            int NetworkCount() const { return this->m_networks.size(); }
            QString NetworkName(int i) const;
            QString NetworkType(int i) const;
            int NetworkLinkSpeedMbps(int i) const;
            QString NetworkIpv4(int i) const;
            QString NetworkIpv6(int i) const;
            double NetworkRxBytesPerSec(int i) const;
            double NetworkTxBytesPerSec(int i) const;
            double NetworkMaxThroughputBytesPerSec(int i) const;
            const HistoryBuffer &NetworkRxHistory(int i) const;
            const HistoryBuffer &NetworkTxHistory(int i) const;

            // ── GPUs (tooling-backed, currently NVML runtime-loaded) ─────────────
            int GpuCount() const { return this->m_gpus.size(); }
            QString GpuName(int i) const;
            QString GpuDriverVersion(int i) const;
            QString GpuBackendName(int i) const;
            double GpuUtilPercent(int i) const;
            int GpuTemperatureC(int i) const;
            qint64 GpuMemUsedMiB(int i) const;
            qint64 GpuMemTotalMiB(int i) const;
            const HistoryBuffer &GpuUtilHistory(int i) const;
            const HistoryBuffer &GpuMemUsageHistory(int i) const;
            const HistoryBuffer &GpuCopyTxHistory(int i) const;
            const HistoryBuffer &GpuCopyRxHistory(int i) const;
            double GpuMaxCopyBytesPerSec(int i) const;
            qint64 GpuSharedMemUsedMiB(int i) const;
            qint64 GpuSharedMemTotalMiB(int i) const;
            const HistoryBuffer &GpuSharedMemHistory(int i) const;
            int GpuEngineCount(int gpuIndex) const;
            QString GpuEngineName(int gpuIndex, int engineIndex) const;
            double GpuEnginePercent(int gpuIndex, int engineIndex) const;
            const HistoryBuffer &GpuEngineHistory(int gpuIndex, int engineIndex) const;

        signals:
            void updated();

        private slots:
            void onTimer();

        private:
            /// Per-core rolling sample state.
            struct CoreSample
            {
                quint64        prevIdle   { 0 };
                quint64        prevTotal  { 0 };
                quint64        prevKernel { 0 };
                HistoryBuffer history { HISTORY_SIZE };
                HistoryBuffer kernelHistory { HISTORY_SIZE };
            };

            struct DiskSample
            {
                QString        name;          ///< base device name (e.g. sda, nvme0n1)
                QString        model;         ///< best-effort model string
                QString        type;          ///< HDD/SSD/Unknown
                quint64        prevReadSecs  { 0 };
                quint64        prevWriteSecs { 0 };
                quint64        prevIoMs      { 0 };
                double         activePct     { 0.0 };
                double         readBps       { 0.0 };
                double         writeBps      { 0.0 };
                double         maxTransferBps { 0.0 };
                qint64         capacityBytes { 0 };
                qint64         formattedBytes { 0 };
                bool           isSystemDisk { false };
                bool           hasPageFile { false };
                HistoryBuffer activeHistory { HISTORY_SIZE };
                HistoryBuffer readHistory { HISTORY_SIZE };
                HistoryBuffer writeHistory { HISTORY_SIZE };
            };

            struct GpuEngineSample
            {
                QString         key;
                QString         label;
                double          pct { 0.0 };
                HistoryBuffer history { HISTORY_SIZE };
            };

            struct GpuSample
            {
                QString         id;
                QString         name;
                QString         driverVersion;
                QString         backend;
                double          utilPct { 0.0 };
                int             temperatureC { -1 };
                qint64          memUsedMiB { 0 };
                qint64          memTotalMiB { 0 };
                qint64          sharedMemUsedMiB { 0 };
                qint64          sharedMemTotalMiB { 0 };
                double          copyTxBps { 0.0 };
                double          copyRxBps { 0.0 };
                double          maxCopyBps { 0.0 };
                HistoryBuffer utilHistory { HISTORY_SIZE };
                HistoryBuffer memUsageHistory { HISTORY_SIZE };
                HistoryBuffer sharedMemHistory { HISTORY_SIZE };
                HistoryBuffer copyTxHistory { HISTORY_SIZE };
                HistoryBuffer copyRxHistory { HISTORY_SIZE };
                QVector<GpuEngineSample> engines;
                QHash<QString, qint64> prevFdInfoEngineNs;
            };

            // Cached sysfs paths for a kernel DRM-managed GPU.
            // Populated once at startup; used every sampling tick.
            struct DrmCard
            {
                QString   id;             // PCI address, e.g. 0000:05:00.0
                QString   vendor;         // PCI vendor, e.g. "0x1002"
                QString   driverName;     // e.g. "amdgpu", "i915"
                QString   driverVersion;
                QString   busyPath;       // .../gpu_busy_percent
                QString   vramTotalPath;  // .../mem_info_vram_total
                QString   vramUsedPath;   // .../mem_info_vram_used
                QString   gttTotalPath;   // .../mem_info_gtt_total  (shared / system)
                QString   gttUsedPath;    // .../mem_info_gtt_used
                QString   tempPath;       // hwmon temp1_input (milli-°C)
                QString   renderNodePath; // /dev/dri/renderDN  (for fdinfo matching)
                QString   cardNodePath;   // /dev/dri/cardN

                // All *_busy_percent engine files (excluding gpu_busy_percent).
                QVector<QPair<QString, QString>>  engineBusyPaths; // (key, sysfs path)

                // Cached fdinfo paths from the last full /proc scan.
                QStringList     cachedFdInfoPaths;
            };

            struct NetworkSample
            {
                QString         name;      ///< Interface name, e.g. enp5s0
                QString         type;      ///< Ethernet/Wi-Fi/Other
                QString         ipv4;
                QString         ipv6;
                bool            isActive { true };
                int             linkSpeedMbps { 0 };
                quint64         prevRxBytes { 0 };
                quint64         prevTxBytes { 0 };
                double          rxBps { 0.0 };
                double          txBps { 0.0 };
                double          maxThroughputBps { 0.0 };
                HistoryBuffer   rxHistory { HISTORY_SIZE };
                HistoryBuffer   txHistory { HISTORY_SIZE };
            };

            QTimer *m_timer;
            int     m_intervalMs { 1000 };
            bool    m_active { true };
            bool    m_processStatsEnabled { false };
            bool    m_cpuSamplingEnabled { true };
            bool    m_memorySamplingEnabled { true };
            bool    m_swapSamplingEnabled { true };
            bool    m_diskSamplingEnabled { true };
            bool    m_networkSamplingEnabled { true };
            bool    m_gpuSamplingEnabled { true };

            // Aggregate CPU state
            quint64          m_prevCpuIdle   { 0 };
            quint64          m_prevCpuTotal  { 0 };
            quint64          m_prevCpuKernel { 0 };
            HistoryBuffer    m_cpuHistory       { HISTORY_SIZE };
            HistoryBuffer    m_cpuKernelHistory { HISTORY_SIZE };

            // Per-core state
            QVector<CoreSample> m_cores;

            // CPU metadata
            QString  m_cpuModelName;
            double   m_cpuBaseMhz       { 0.0 };
            double   m_cpuCurrentMhz    { 0.0 };
            QVector<double> m_coreCurrentMhz;
            int      m_cpuLogicalCount  { 0 };
            bool     m_cpuIsVirtualMachine { false };
            QString  m_cpuVmVendor;
            int      m_cpuTemperatureC { -1 };
            QString  m_cpuTempInputPath;
            bool     m_cpuFreqSourceDetected { false };
            bool     m_cpuFreqUseSysfs { false };
            QVector<QString> m_cpuFreqPaths;

            // Process/thread counts
            int      m_processCount { 0 };
            int      m_threadCount  { 0 };

            // Memory state
            qint64           m_memTotalKb   { 0 };
            qint64           m_memUsedKb    { 0 };
            qint64           m_memAvailKb   { 0 };
            qint64           m_memFreeKb    { 0 };
            qint64           m_memCachedKb  { 0 };
            qint64           m_memBuffersKb { 0 };
            qint64           m_memDirtyKb   { 0 };
            int              m_memDimmSlotsTotal { 0 };
            int              m_memDimmSlotsUsed  { 0 };
            int              m_memSpeedMtps      { 0 };
            HistoryBuffer    m_memHistory { HISTORY_SIZE };

            // Swap state
            qint64           m_swapTotalKb   { 0 };
            qint64           m_swapUsedKb    { 0 };
            qint64           m_swapFreeKb    { 0 };
            quint64          m_prevSwapInPages  { 0 };
            quint64          m_prevSwapOutPages { 0 };
            double           m_swapInBps  { 0.0 };
            double           m_swapOutBps { 0.0 };
            double           m_swapMaxActivityBps { 0.0 };
            HistoryBuffer    m_swapUsageHistory { HISTORY_SIZE };
            HistoryBuffer    m_swapInHistory { HISTORY_SIZE };
            HistoryBuffer    m_swapOutHistory { HISTORY_SIZE };
            QElapsedTimer    m_swapTimer;
            qint64           m_prevSwapSampleMs { 0 };

            // Disk state
            QVector<DiskSample> m_disks;
            QElapsedTimer       m_diskTimer;
            qint64              m_prevDiskSampleMs { 0 };

            // Network state
            QVector<NetworkSample> m_networks;
            QElapsedTimer          m_netTimer;
            qint64                 m_prevNetSampleMs { 0 };
            int                    m_networkStateRefreshCounter { 0 };
            int                    m_networkMetadataRefreshCounter { 0 };

            // GPU state
            QVector<GpuSample>  m_gpus;
            bool                m_hasNvml { false };
            void               *m_nvmlLibHandle { nullptr };
            QVector<DrmCard>    m_drmCards;
            QElapsedTimer       m_gpuFdInfoTimer;
            bool                m_gpuFdInfoTimerStarted { false };
            int                 m_gpuFdInfoRescanCounter { 0 };

            /// Sample /proc/stat aggregate + per-core jiffies and append utilization histories.
            bool sampleCpu();
            /// Sample /proc/meminfo (+ /proc/vmstat for swap activity) and append memory/swap histories.
            bool sampleMemory();
            /// Sample /proc/diskstats and compute per-device active time % and read/write throughput.
            bool sampleDisks();
            /// Sample /proc/net/dev counters and compute per-interface RX/TX throughput histories.
            bool sampleNetworks();
            void refreshNetworkState(bool force = false);
            void refreshNetworkMetadata(bool force = false);
            /// Sample GPU backends (NVML when available) and update utilization/memory/engine histories.
            bool sampleGpus();
            /// Count processes/threads via /proc walk for CPU detail statistics.
            void sampleProcessStats();
            void readCpuMetadata();
            void detectCpuFreqSource();
            void readCurrentFreq();
            void readHardwareMetadata();
            void detectCpuTemperatureSensor();
            void sampleCpuTemperature();
            void refreshDisks(const QSet<QString> &measurableDevices);
            void detectGpuBackends();
            void detectDrmCards();
            bool sampleNvml();
            bool sampleDrm();
            //! Run all samples that are enabled
            void sample();

            QHash<QString, qint64> scanDrmFdInfoEngines(DrmCard &card);
            void unloadGpuBackends();
            static double parsePercentField(const QString &field);
            static qint64 parseMiBField(const QString &field);

            static QStringList listTrackedBlockDevices(const QSet<QString> &measurableDevices);
            static QSet<QString> resolveBaseBlockDevices(const QString &devName);
            static bool shouldIgnoreBlockDevice(const QString &baseName);
            static QString readSysTextFile(const QString &path);
            static bool isActiveNetworkInterface(const QString &name);
            static QString networkTypeFromArpType(int arpType);
            static int readLinkSpeedMbps(const QString &name);
            static quint16 readLe16(const QByteArray &raw, int off);
            static quint32 readLe32(const QByteArray &raw, int off);

            static void   appendHistory(HistoryBuffer &vec, double val);
            static void   appendHistoryAndUpdateMax(HistoryBuffer &vec, double val, double &cachedMax, double minMax = 0.0);
            static quint64 parseCpuLine(const QList<QByteArray> &parts,  quint64 &outIdle, quint64 &outKernel);
    };
} // namespace Perf

#endif // PERF_PERFDATAPROVIDER_H
