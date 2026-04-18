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

#include "processmodel.h"
#include "proc.h"
#include "../configuration.h"
#include "../misc.h"
#include <unistd.h>

using namespace OS;

// ── Construction ──────────────────────────────────────────────────────────────

ProcessModel::ProcessModel(QObject *parent) : QAbstractTableModel(parent)
{
    this->m_numCpus = static_cast<int>(sysconf(_SC_NPROCESSORS_ONLN));
    if (this->m_numCpus < 1)
        this->m_numCpus = 1;
    this->m_myUid = ::getuid();
}

// ── QAbstractTableModel interface ─────────────────────────────────────────────

int ProcessModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return this->m_processes.size();
}

int ProcessModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return ColCount;
}

QVariant ProcessModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()
        || index.row()    < 0
        || index.row()    >= this->m_processes.size()
        || index.column() < 0
        || index.column() >= ColCount)
        return {};

    const Process &proc = this->m_processes.at(index.row());

    if (role == Qt::DisplayRole)
    {
        switch (static_cast<Column>(index.column()))
        {
            case ColPid:      return proc.PID;
            case ColName:     return proc.Name;
            case ColUser:     return proc.User;
            case ColState:    return Process::GetStateString(proc.State);
            case ColCpu:      return QString::number(proc.CPUPercent, 'f', 1) + " %";
            case ColMemRss:   return Misc::FormatKiB(proc.VMRssKb, 0);
            case ColMemVirt:  return Misc::FormatKiB(proc.vmSizeKb, 0);
            case ColIoReads:  return proc.IOTotalsAvailable ? Misc::FormatBytes(proc.IOReadBytes, 0) : QString("?");
            case ColIoWrites: return proc.IOTotalsAvailable ? Misc::FormatBytes(proc.IOWriteBytes, 0) : QString("?");
            case ColIoReadsPerSec:
                if (proc.IOPermissionDenied)
                    return QString("?");
                return proc.IORatesAvailable ? Misc::FormatBytesPerSecond(proc.IOReadBps) : tr("measuring...");
            case ColIoWritesPerSec:
                if (proc.IOPermissionDenied)
                    return QString("?");
                return proc.IORatesAvailable ? Misc::FormatBytesPerSecond(proc.IOWriteBps) : tr("measuring...");
            case ColThreads:  return proc.Threads;
            case ColPriority: return proc.Priority;
            case ColNice:     return proc.Nice;
            case ColCmdline:  return proc.CmdLine;
            default: break;
        }
    }

    // Raw numeric values for sorting
    if (role == Qt::UserRole)
    {
        switch (static_cast<Column>(index.column()))
        {
            case ColPid:      return static_cast<qlonglong>(proc.PID);
            case ColCpu:      return proc.CPUPercent;
            case ColMemRss:   return static_cast<qulonglong>(proc.VMRssKb);
            case ColMemVirt:  return static_cast<qulonglong>(proc.vmSizeKb);
            case ColIoReads:
                return proc.IOTotalsAvailable
                       ? QVariant::fromValue(static_cast<qlonglong>(proc.IOReadBytes))
                       : QVariant::fromValue(static_cast<qlonglong>(-1));
            case ColIoWrites:
                return proc.IOTotalsAvailable
                       ? QVariant::fromValue(static_cast<qlonglong>(proc.IOWriteBytes))
                       : QVariant::fromValue(static_cast<qlonglong>(-1));
            case ColIoReadsPerSec:
                return proc.IORatesAvailable
                       ? QVariant::fromValue(proc.IOReadBps)
                       : QVariant::fromValue(-1.0);
            case ColIoWritesPerSec:
                return proc.IORatesAvailable
                       ? QVariant::fromValue(proc.IOWriteBps)
                       : QVariant::fromValue(-1.0);
            case ColThreads:  return proc.Threads;
            case ColPriority: return proc.Priority;
            case ColNice:     return proc.Nice;
            default:          return this->data(index, Qt::DisplayRole);
        }
    }

    if (role == Qt::TextAlignmentRole)
    {
        switch (static_cast<Column>(index.column()))
        {
            case ColPid:
            case ColCpu:
            case ColMemRss:
            case ColMemVirt:
            case ColIoReads:
            case ColIoWrites:
            case ColIoReadsPerSec:
            case ColIoWritesPerSec:
            case ColThreads:
            case ColPriority:
            case ColNice:
                return QVariant(Qt::AlignRight | Qt::AlignVCenter);
            default:
                return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
        }
    }

    return {};
}

QVariant ProcessModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};
    if (section < 0 || section >= ColCount)
        return {};
    return columnHeader(static_cast<Column>(section));
}

Qt::ItemFlags ProcessModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

// ── Refresh ───────────────────────────────────────────────────────────────────

void ProcessModel::Refresh()
{
    QList<Process> fresh = this->RefreshSnapshot();

    this->SetProcesses(fresh);
}

void ProcessModel::SetProcesses(const QList<Process> &processes)
{
    this->beginResetModel();
    this->m_processes = processes;
    this->endResetModel();
}

QList<Process> ProcessModel::RefreshSnapshot()
{
    // Read total elapsed CPU jiffies (all CPUs, all states) from /proc/stat.
    // Using the actual CPU time budget as the denominator — rather than wall
    // clock time — matches htop's approach and gives accurate results even
    // when the timer fires slightly early or late.
    const quint64 totalJiffies = Proc::ReadTotalCpuJiffies();
    const quint64 periodJiffies =
        (this->m_prevCpuTotalTicks > 0 && totalJiffies > this->m_prevCpuTotalTicks)
        ? (totalJiffies - this->m_prevCpuTotalTicks) : 0;

    Process::LoadOptions opts;
    opts.IncludeKernelTasks = this->m_showKernelTasks;
    opts.IncludeOtherUsers  = this->m_showOtherUsersProcs;
    opts.MyUID              = this->m_myUid;
    opts.CollectIOMetrics   = this->m_ioMetricsEnabled;
    opts.IsSuperuser        = CFG->IsSuperuser;
    opts.EffectiveUID       = CFG->EUID;
    QList<Process> fresh = Process::LoadAll(opts);

    // Calculate CPU% per process: (delta process ticks) / (period per CPU) * 100
    if (periodJiffies > 0)
    {
        const double periodPerCpu =
            static_cast<double>(periodJiffies) / this->m_numCpus;

        for (Process &proc : fresh)
        {
            if (this->m_prevTicks.contains(proc.PID))
            {
                const quint64 prevTicks = this->m_prevTicks.value(proc.PID);
                if (proc.CPUTicks >= prevTicks)
                {
                    const double pct = static_cast<double>(proc.CPUTicks - prevTicks) / periodPerCpu * 100.0;
                    // Cap at 100 % × num_cpus (matches htop's MINIMUM() clamp)
                    proc.CPUPercent = qMin(pct, 100.0 * this->m_numCpus);
                }
            }
        }
    }

    const qint64 ioElapsedMs = this->m_ioMetricsEnabled
                               ? (this->m_prevIoSampleTimer.isValid() ? this->m_prevIoSampleTimer.restart() : -1)
                               : -1;
    if (this->m_ioMetricsEnabled && !this->m_prevIoSampleTimer.isValid())
        this->m_prevIoSampleTimer.start();

    if (this->m_ioMetricsEnabled && ioElapsedMs > 0)
    {
        const double elapsedSec = static_cast<double>(ioElapsedMs) / 1000.0;
        for (Process &proc : fresh)
        {
            if (!proc.IOTotalsAvailable)
                continue;
            if (!this->m_prevIoReadBytes.contains(proc.PID) || !this->m_prevIoWriteBytes.contains(proc.PID))
                continue;

            const quint64 prevReadBytes = this->m_prevIoReadBytes.value(proc.PID);
            const quint64 prevWriteBytes = this->m_prevIoWriteBytes.value(proc.PID);
            if (proc.IOReadBytes < prevReadBytes || proc.IOWriteBytes < prevWriteBytes)
                continue;

            proc.IOReadBps = static_cast<double>(proc.IOReadBytes - prevReadBytes) / elapsedSec;
            proc.IOWriteBps = static_cast<double>(proc.IOWriteBytes - prevWriteBytes) / elapsedSec;
            proc.IORatesAvailable = true;
        }
    }

    // Store snapshots for next sample
    this->m_prevTicks.clear();
    for (const Process &proc : fresh)
        this->m_prevTicks.insert(proc.PID, proc.CPUTicks);
    this->m_prevCpuTotalTicks = totalJiffies;

    if (this->m_ioMetricsEnabled)
    {
        this->m_prevIoReadBytes.clear();
        this->m_prevIoWriteBytes.clear();
        for (const Process &proc : fresh)
        {
            if (!proc.IOTotalsAvailable)
                continue;
            this->m_prevIoReadBytes.insert(proc.PID, proc.IOReadBytes);
            this->m_prevIoWriteBytes.insert(proc.PID, proc.IOWriteBytes);
        }
        if (!this->m_prevIoSampleTimer.isValid())
            this->m_prevIoSampleTimer.start();
    }

    return fresh;
}

void ProcessModel::SetIOMetricsEnabled(bool enabled)
{
    if (this->m_ioMetricsEnabled == enabled)
        return;

    this->m_ioMetricsEnabled = enabled;
    this->FlushIOMetrics();
}

void ProcessModel::FlushIOMetrics()
{
    this->m_prevIoReadBytes.clear();
    this->m_prevIoWriteBytes.clear();
    this->m_prevIoSampleTimer.invalidate();
}

// ── Helpers ───────────────────────────────────────────────────────────────────

QString ProcessModel::columnHeader(Column col)
{
    switch (col)
    {
        case ColPid:      return "PID";
        case ColName:     return "Name";
        case ColUser:     return "User";
        case ColState:    return "State";
        case ColCpu:      return "CPU %";
        case ColMemRss:   return "MEM RES";
        case ColMemVirt:  return "MEM VIRT";
        case ColIoReads:  return "IO Reads";
        case ColIoWrites: return "IO Writes";
        case ColIoReadsPerSec: return "IO Read/s";
        case ColIoWritesPerSec:return "IO Write/s";
        case ColThreads:  return "Threads";
        case ColPriority: return "Priority";
        case ColNice:     return "Nice";
        case ColCmdline:  return "Command";
        default:          return {};
    }
}
