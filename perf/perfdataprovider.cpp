#include "perfdataprovider.h"

#include "logger.h"

#include <QDir>
#include <QFile>
#include <unistd.h>

namespace Perf
{

// ── Construction ──────────────────────────────────────────────────────────────

PerfDataProvider::PerfDataProvider(QObject *parent)
    : QObject(parent)
    , m_timer(new QTimer(this))
{
    connect(this->m_timer, &QTimer::timeout, this, &PerfDataProvider::onTimer);

    this->readCpuMetadata();

    // Prime CPU baseline — first real sample will have a valid delta
    this->sampleCpu();
    this->sampleMemory();
    this->sampleProcessStats();

    this->m_timer->start(1000);
}

void PerfDataProvider::setInterval(int ms)
{
    this->m_timer->setInterval(ms);
}

double PerfDataProvider::memFraction() const
{
    if (this->m_memTotalKb <= 0)
        return 0.0;
    return static_cast<double>(this->m_memUsedKb)
           / static_cast<double>(this->m_memTotalKb);
}

double PerfDataProvider::corePercent(int i) const
{
    if (i < 0 || i >= this->m_cores.size())
        return 0.0;
    const auto &c = this->m_cores.at(i);
    return c.history.isEmpty() ? 0.0 : c.history.last();
}

const QVector<double> &PerfDataProvider::coreHistory(int i) const
{
    static const QVector<double> empty;
    if (i < 0 || i >= this->m_cores.size())
        return empty;
    return this->m_cores.at(i).history;
}

const QVector<double> &PerfDataProvider::coreKernelHistory(int i) const
{
    static const QVector<double> empty;
    if (i < 0 || i >= this->m_cores.size())
        return empty;
    return this->m_cores.at(i).kernelHistory;
}

// ── Private slots ─────────────────────────────────────────────────────────────

void PerfDataProvider::onTimer()
{
    this->sampleCpu();
    this->sampleMemory();
    this->sampleProcessStats();
    this->readCurrentFreq();
    emit this->updated();
}

// ── CPU sampling ──────────────────────────────────────────────────────────────

// Parse one "cpu..." line from /proc/stat — returns total jiffies and writes
// idle and kernel jiffies via output parameters.
quint64 PerfDataProvider::parseCpuLine(const QList<QByteArray> &parts,
                                       quint64 &outIdle,
                                       quint64 &outKernel)
{
    // Fields (1-indexed after the label):
    // 1:user 2:nice 3:system 4:idle 5:iowait 6:irq 7:softirq 8:steal
    // guest/guestnice are already counted in user/nice — skip them
    const quint64 user    = parts.value(1).toULongLong();
    const quint64 nice    = parts.value(2).toULongLong();
    const quint64 system  = parts.value(3).toULongLong();
    const quint64 idle    = parts.value(4).toULongLong();
    const quint64 iowait  = parts.value(5).toULongLong();
    const quint64 irq     = parts.value(6).toULongLong();
    const quint64 softirq = parts.value(7).toULongLong();
    const quint64 steal   = parts.value(8).toULongLong();

    outIdle   = idle + iowait;
    outKernel = system + irq + softirq;
    return user + nice + outKernel + outIdle + steal;
}

bool PerfDataProvider::sampleCpu()
{
    QFile f("/proc/stat");
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    int coreIdx = 0;

    while (!f.atEnd())
    {
        const QByteArray raw   = f.readLine();
        const QList<QByteArray> parts = raw.simplified().split(' ');
        if (parts.isEmpty())
            continue;

        const QByteArray key = parts.at(0);

        if (key == "cpu")
        {
            // Aggregate line
            quint64 idleAll = 0, kernelAll = 0;
            const quint64 total = parseCpuLine(parts, idleAll, kernelAll);

            const quint64 dTotal  = (total     > this->m_prevCpuTotal ) ? (total     - this->m_prevCpuTotal ) : 0;
            const quint64 dIdle   = (idleAll   > this->m_prevCpuIdle  ) ? (idleAll   - this->m_prevCpuIdle  ) : 0;
            const quint64 dKernel = (kernelAll > this->m_prevCpuKernel) ? (kernelAll - this->m_prevCpuKernel) : 0;

            double pct = 0.0, kpct = 0.0;
            if (dTotal > 0)
            {
                pct  = (1.0 - static_cast<double>(dIdle)   / static_cast<double>(dTotal)) * 100.0;
                kpct =        static_cast<double>(dKernel) / static_cast<double>(dTotal)  * 100.0;
            }

            this->m_prevCpuTotal  = total;
            this->m_prevCpuIdle   = idleAll;
            this->m_prevCpuKernel = kernelAll;
            appendHistory(this->m_cpuHistory,       pct);
            appendHistory(this->m_cpuKernelHistory, kpct);
        }
        else if (key.startsWith("cpu") && key.size() > 3)
        {
            // Per-core line: "cpu0", "cpu1", ...
            if (coreIdx >= this->m_cores.size())
                this->m_cores.resize(coreIdx + 1);

            CoreSample &c = this->m_cores[coreIdx];
            quint64 idleC = 0, kernelC = 0;
            const quint64 totalC = parseCpuLine(parts, idleC, kernelC);

            const quint64 dTotal  = (totalC  > c.prevTotal ) ? (totalC  - c.prevTotal ) : 0;
            const quint64 dIdle   = (idleC   > c.prevIdle  ) ? (idleC   - c.prevIdle  ) : 0;
            const quint64 dKernel = (kernelC > c.prevKernel) ? (kernelC - c.prevKernel) : 0;

            double pct = 0.0, kpct = 0.0;
            if (dTotal > 0)
            {
                pct  = (1.0 - static_cast<double>(dIdle)   / static_cast<double>(dTotal)) * 100.0;
                kpct =        static_cast<double>(dKernel) / static_cast<double>(dTotal)  * 100.0;
            }

            c.prevTotal  = totalC;
            c.prevIdle   = idleC;
            c.prevKernel = kernelC;
            appendHistory(c.history,       pct);
            appendHistory(c.kernelHistory, kpct);
            ++coreIdx;
        }
        else if (coreIdx > 0)
        {
            break;   // past cpu lines — stop reading for efficiency
        }
    }

    f.close();
    return true;
}

// ── Memory sampling ───────────────────────────────────────────────────────────

bool PerfDataProvider::sampleMemory()
{
    QFile f("/proc/meminfo");
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    qint64 memTotal = 0, memAvail = 0, memFree = 0;
    qint64 buffers  = 0, cached   = 0, sReclaimable = 0, shmem = 0;
    qint64 dirty    = 0, writeback = 0;

    while (!f.atEnd())
    {
        const QByteArray line  = f.readLine();
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
    }
    f.close();

    // htop formula: used = total - free - buffers - page_cache
    // where page_cache = Cached + SReclaimable - Shmem
    const qint64 pageCache = cached + sReclaimable - shmem;

    if (memTotal <= 0)
        LOG_WARN("meminfo parse produced MemTotal <= 0");

    LOG_INFO(QString("meminfo: total=%1 free=%2 avail=%3 bufs=%4 cached=%5 srec=%6 shmem=%7 dirty=%8 wb=%9")
             .arg(memTotal).arg(memFree).arg(memAvail)
             .arg(buffers).arg(cached).arg(sReclaimable)
             .arg(shmem).arg(dirty).arg(writeback));

    this->m_memTotalKb   = memTotal;
    this->m_memAvailKb   = memAvail;
    this->m_memFreeKb    = memFree;
    this->m_memBuffersKb = buffers;
    this->m_memDirtyKb   = dirty + writeback;
    // Full page cache including buffers (what we show in stats and composition bar)
    this->m_memCachedKb  = buffers + pageCache;
    // htop's "used" (processes' non-reclaimable footprint)
    this->m_memUsedKb    = qMax(0LL, memTotal - memFree - buffers - pageCache);

    // Graph tracks used / total (htop formula matches the green bar)
    const double frac = (memTotal > 0)
                        ? static_cast<double>(this->m_memUsedKb) / static_cast<double>(memTotal)
                        : 0.0;
    appendHistory(this->m_memHistory, frac * 100.0);
    return true;
}

// ── Process / thread counts ───────────────────────────────────────────────────

void PerfDataProvider::sampleProcessStats()
{
    int procs = 0, threads = 0;
    const QDir procDir("/proc");
    for (const QString &entry : procDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot))
    {
        bool ok;
        entry.toInt(&ok);
        if (!ok)
            continue;
        ++procs;
        // Each thread appears as a subdirectory entry under /proc/<pid>/task/
        const QDir taskDir(QString("/proc/%1/task").arg(entry));
        // count() includes "." and ".." so subtract 2
        threads += static_cast<int>(taskDir.count()) - 2;
    }
    this->m_processCount = procs;
    this->m_threadCount  = qMax(0, threads);
}

// ── CPU metadata ──────────────────────────────────────────────────────────────

void PerfDataProvider::readCpuMetadata()
{
    this->m_cpuLogicalCount = static_cast<int>(sysconf(_SC_NPROCESSORS_ONLN));

    QFile f("/proc/cpuinfo");
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    bool gotModel = false, gotBase = false;
    while (!f.atEnd())
    {
        const QByteArray line  = f.readLine();
        const int        colon = line.indexOf(':');
        if (colon < 0)
            continue;
        const QByteArray key = line.left(colon).trimmed();
        const QByteArray val = line.mid(colon + 1).trimmed();

        if (!gotModel && key == "model name")
        {
            this->m_cpuModelName = QString::fromUtf8(val);
            gotModel = true;
        }
        else if (!gotBase && key == "cpu MHz")
        {
            this->m_cpuBaseMhz = val.toDouble();
            gotBase = true;
        }

        if (gotModel && gotBase)
            break;
    }
    f.close();

    this->readCurrentFreq();
}

void PerfDataProvider::readCurrentFreq()
{
    // sysfs gives the actual current frequency per CPU0
    QFile sf("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq");
    if (sf.open(QIODevice::ReadOnly))
    {
        const double kHz = sf.readAll().trimmed().toDouble();
        sf.close();
        if (kHz > 0.0)
        {
            this->m_cpuCurrentMhz = kHz / 1000.0;
            return;
        }
    }
    // Fallback: first "cpu MHz" line from /proc/cpuinfo
    QFile f("/proc/cpuinfo");
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return;
    while (!f.atEnd())
    {
        const QByteArray line  = f.readLine();
        const int        colon = line.indexOf(':');
        if (colon < 0)
            continue;
        if (line.left(colon).trimmed() == "cpu MHz")
        {
            this->m_cpuCurrentMhz = line.mid(colon + 1).trimmed().toDouble();
            break;
        }
    }
    f.close();
}

// ── Helpers ───────────────────────────────────────────────────────────────────

// static
void PerfDataProvider::appendHistory(QVector<double> &vec, double val)
{
    vec.append(val);
    while (vec.size() > HISTORY_SIZE)
        vec.removeFirst();
}

} // namespace Perf
