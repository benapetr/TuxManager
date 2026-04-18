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

#ifndef OS_PROCESSMODEL_H
#define OS_PROCESSMODEL_H

#include "process.h"

#include <QAbstractTableModel>
#include <QElapsedTimer>
#include <QHash>

namespace OS
{
    class ProcessModel : public QAbstractTableModel
    {
        Q_OBJECT

        public:
            /// Logical columns — order in the enum defines default display order.
            enum Column
            {
                ColPid = 0,
                ColName,
                ColUser,
                ColState,
                ColCpu,
                ColMemRss,
                ColMemVirt,
                ColIoReads,
                ColIoWrites,
                ColIoReadsPerSec,
                ColIoWritesPerSec,
                ColThreads,
                ColPriority,
                ColNice,
                ColCmdline,
                ColCount           ///< Sentinel — always last
            };

            explicit ProcessModel(QObject *parent = nullptr);

            // QAbstractTableModel interface
            int      rowCount(const QModelIndex &parent = QModelIndex()) const override;
            int      columnCount(const QModelIndex &parent = QModelIndex()) const override;
            QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
            QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
            Qt::ItemFlags flags(const QModelIndex &index) const override;

            /// Reload all process data from /proc and update CPU%.
            /// Call this on a timer tick.
            void Refresh();
            /// Reload process data and return it without resetting the table model.
            /// Useful for alternate views (e.g. tree) to avoid unnecessary table updates.
            QList<Process> RefreshSnapshot();
            void SetProcesses(const QList<Process> &processes);
            void SetShowKernelTasks(bool show) { this->m_showKernelTasks = show; }
            void SetShowOtherUsersProcs(bool show) { this->m_showOtherUsersProcs = show; }
            void SetIOMetricsEnabled(bool enabled);
            void FlushIOMetrics();

            /// The raw process list (read-only access for external use).
            const QList<Process> &GetProcesses() const { return this->m_processes; }

        private:
            QList<Process>          m_processes;
            QHash<pid_t, quint64>   m_prevTicks;               ///< cpuTicks from previous sample
            QHash<pid_t, quint64>   m_prevIoReadBytes;
            QHash<pid_t, quint64>   m_prevIoWriteBytes;
            quint64                 m_prevCpuTotalTicks { 0 }; ///< Total CPU jiffies from previous /proc/stat sample
            QElapsedTimer           m_prevIoSampleTimer;
            int                     m_numCpus { 1 };           ///< Online CPU count for normalisation
            uid_t                   m_myUid { 0 };
            bool                    m_showKernelTasks { true };
            bool                    m_showOtherUsersProcs { true };
            bool                    m_ioMetricsEnabled { false };

            static QString columnHeader(Column col);
    };
} // namespace Os

#endif // OS_PROCESSMODEL_H
