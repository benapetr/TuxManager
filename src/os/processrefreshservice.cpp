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

#include "processrefreshservice.h"

#include "proc.h"

#include <unistd.h>
#include <utility>

using namespace OS;

ProcessRefreshWorker::ProcessRefreshWorker(QObject *parent) : QObject(parent)
{
    this->m_numCpus = static_cast<int>(::sysconf(_SC_NPROCESSORS_ONLN));
    if (this->m_numCpus < 1)
        this->m_numCpus = 1;
}

void ProcessRefreshWorker::fetch(int consumer,
                                 quint64 token,
                                 bool includeKernelTasks,
                                 bool includeOtherUsers,
                                 bool collectIOMetrics,
                                 bool isSuperuser,
                                 uint effectiveUid,
                                 uint myUid)
{
    const quint64 totalJiffies = Proc::ReadTotalCpuJiffies();
    const quint64 periodJiffies =
        (this->m_prevCpuTotalTicks > 0 && totalJiffies > this->m_prevCpuTotalTicks)
        ? (totalJiffies - this->m_prevCpuTotalTicks) : 0;

    Process::LoadOptions opts;
    opts.IncludeKernelTasks = includeKernelTasks;
    opts.IncludeOtherUsers = includeOtherUsers;
    opts.MyUID = static_cast<uid_t>(myUid);
    opts.CollectIOMetrics = collectIOMetrics;
    opts.IsSuperuser = isSuperuser;
    opts.EffectiveUID = static_cast<uid_t>(effectiveUid);

    QList<Process> fresh = Process::LoadAll(opts);

    if (periodJiffies > 0)
    {
        const double periodPerCpu = static_cast<double>(periodJiffies) / this->m_numCpus;

        for (Process &proc : fresh)
        {
            const auto it = this->m_prevTicks.constFind(proc.PID);
            if (it == this->m_prevTicks.cend() || proc.CPUTicks < it.value())
                continue;

            const double pct = static_cast<double>(proc.CPUTicks - it.value()) / periodPerCpu * 100.0;
            proc.CPUPercent = qMin(pct, 100.0 * this->m_numCpus);
        }
    }

    const qint64 ioElapsedMs = collectIOMetrics
                               ? (this->m_prevIoSampleTimer.isValid() ? this->m_prevIoSampleTimer.restart() : -1)
                               : -1;
    if (collectIOMetrics && !this->m_prevIoSampleTimer.isValid())
        this->m_prevIoSampleTimer.start();

    if (collectIOMetrics && ioElapsedMs > 0)
    {
        const double elapsedSec = static_cast<double>(ioElapsedMs) / 1000.0;
        for (Process &proc : fresh)
        {
            if (!proc.IOTotalsAvailable)
                continue;

            const auto readIt = this->m_prevIoReadBytes.constFind(proc.PID);
            const auto writeIt = this->m_prevIoWriteBytes.constFind(proc.PID);
            if (readIt == this->m_prevIoReadBytes.cend() || writeIt == this->m_prevIoWriteBytes.cend())
                continue;
            if (proc.IOReadBytes < readIt.value() || proc.IOWriteBytes < writeIt.value())
                continue;

            proc.IOReadBps = static_cast<double>(proc.IOReadBytes - readIt.value()) / elapsedSec;
            proc.IOWriteBps = static_cast<double>(proc.IOWriteBytes - writeIt.value()) / elapsedSec;
            proc.IORatesAvailable = true;
        }
    }

    this->m_prevTicks.clear();
    this->m_prevTicks.reserve(fresh.size());
    for (const Process &proc : std::as_const(fresh))
        this->m_prevTicks.insert(proc.PID, proc.CPUTicks);
    this->m_prevCpuTotalTicks = totalJiffies;

    if (collectIOMetrics)
    {
        this->m_prevIoReadBytes.clear();
        this->m_prevIoWriteBytes.clear();
        for (const Process &proc : std::as_const(fresh))
        {
            if (!proc.IOTotalsAvailable)
                continue;
            this->m_prevIoReadBytes.insert(proc.PID, proc.IOReadBytes);
            this->m_prevIoWriteBytes.insert(proc.PID, proc.IOWriteBytes);
        }
        if (!this->m_prevIoSampleTimer.isValid())
            this->m_prevIoSampleTimer.start();
    } else
    {
        this->flushIOMetrics();
    }

    emit fetched(consumer, token, fresh);
}

void ProcessRefreshWorker::flushIOMetrics()
{
    this->m_prevIoReadBytes.clear();
    this->m_prevIoWriteBytes.clear();
    this->m_prevIoSampleTimer.invalidate();
}

ProcessRefreshService::ProcessRefreshService(QObject *parent)
    : QObject(parent)
    , m_workerThread(new QThread(this))
    , m_worker(new ProcessRefreshWorker())
{
    qRegisterMetaType<OS::Process>("OS::Process");
    qRegisterMetaType<QList<OS::Process>>("QList<OS::Process>");

    this->m_worker->moveToThread(this->m_workerThread);
    connect(this->m_workerThread, &QThread::finished, this->m_worker, &QObject::deleteLater);
    connect(this,
            &ProcessRefreshService::requestRefresh,
            this->m_worker,
            &ProcessRefreshWorker::fetch,
            Qt::QueuedConnection);
    connect(this->m_worker,
            &ProcessRefreshWorker::fetched,
            this,
            &ProcessRefreshService::snapshotReady,
            Qt::QueuedConnection);
    this->m_workerThread->start();
}

ProcessRefreshService::~ProcessRefreshService()
{
    this->m_workerThread->quit();
    this->m_workerThread->wait(1000);
}

void ProcessRefreshService::RequestSnapshot(Consumer consumer, quint64 token, const Process::LoadOptions &options)
{
    emit requestRefresh(static_cast<int>(consumer),
                        token,
                        options.IncludeKernelTasks,
                        options.IncludeOtherUsers,
                        options.CollectIOMetrics,
                        options.IsSuperuser,
                        static_cast<uint>(options.EffectiveUID),
                        static_cast<uint>(options.MyUID));
}
