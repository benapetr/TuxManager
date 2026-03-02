#ifndef PERF_PERFDATAPROVIDER_H
#define PERF_PERFDATAPROVIDER_H

#include <QObject>
#include <QTimer>
#include <QVector>
#include <QString>

namespace Perf
{

/// Number of historical samples kept per metric.
static constexpr int HISTORY_SIZE = 60;

/// Periodically samples /proc/stat (CPU) and /proc/meminfo (memory).
/// All widgets that display performance data connect to the updated() signal
/// and read values through the const accessors.
class PerfDataProvider : public QObject
{
    Q_OBJECT

    public:
        explicit PerfDataProvider(QObject *parent = nullptr);

        void setInterval(int ms);

        // ── Aggregate CPU ─────────────────────────────────────────────────────
        double cpuPercent()  const { return this->m_cpuHistory.isEmpty() ? 0.0
                                            : this->m_cpuHistory.last(); }
        const QVector<double> &cpuHistory()       const { return this->m_cpuHistory; }
        const QVector<double> &cpuKernelHistory() const { return this->m_cpuKernelHistory; }

        // ── Per-core CPU ──────────────────────────────────────────────────────
        int    coreCount()                          const { return this->m_cores.size(); }
        double corePercent(int i)                   const;
        const  QVector<double> &coreHistory(int i)       const;
        const  QVector<double> &coreKernelHistory(int i) const;

        // ── CPU metadata (read once at startup) ───────────────────────────────
        const QString &cpuModelName() const { return this->m_cpuModelName; }
        double cpuBaseMhz()           const { return this->m_cpuBaseMhz;   }
        double cpuCurrentMhz()        const { return this->m_cpuCurrentMhz; }
        int    cpuLogicalCount()      const { return this->m_cpuLogicalCount; }

        // ── Process / thread counts (updated every sample) ────────────────────
        int processCount() const { return this->m_processCount; }
        int threadCount()  const { return this->m_threadCount;  }

        // ── Memory ───────────────────────────────────────────────────────────
        qint64 memTotalKb()   const { return this->m_memTotalKb;   }
        /// In-use (htop formula): Total - Free - Buffers - PageCache
        qint64 memUsedKb()    const { return this->m_memUsedKb;    }
        qint64 memAvailKb()   const { return this->m_memAvailKb;   }
        /// Truly free (MemFree from /proc/meminfo)
        qint64 memFreeKb()    const { return this->m_memFreeKb;    }
        /// Page cache: Cached + SReclaimable - Shmem + Buffers
        qint64 memCachedKb()  const { return this->m_memCachedKb;  }
        qint64 memBuffersKb() const { return this->m_memBuffersKb; }
        /// Dirty pages pending write-back: Dirty + Writeback
        qint64 memDirtyKb()   const { return this->m_memDirtyKb;   }
        double memFraction()  const;
        const QVector<double> &memHistory() const { return this->m_memHistory; }

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
            QVector<double> history;
            QVector<double> kernelHistory;
        };

        QTimer *m_timer;

        // Aggregate CPU state
        quint64          m_prevCpuIdle   { 0 };
        quint64          m_prevCpuTotal  { 0 };
        quint64          m_prevCpuKernel { 0 };
        QVector<double>  m_cpuHistory;
        QVector<double>  m_cpuKernelHistory;

        // Per-core state
        QVector<CoreSample> m_cores;

        // CPU metadata
        QString  m_cpuModelName;
        double   m_cpuBaseMhz       { 0.0 };
        double   m_cpuCurrentMhz    { 0.0 };
        int      m_cpuLogicalCount  { 0 };

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
        QVector<double>  m_memHistory;

        bool sampleCpu();
        bool sampleMemory();
        void sampleProcessStats();
        void readCpuMetadata();
        void readCurrentFreq();

        static void   appendHistory(QVector<double> &vec, double val);
        static quint64 parseCpuLine(const QList<QByteArray> &parts,
                                    quint64 &outIdle, quint64 &outKernel);
};

} // namespace Perf

#endif // PERF_PERFDATAPROVIDER_H
