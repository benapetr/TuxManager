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

#ifndef OS_PROCESS_H
#define OS_PROCESS_H

#include <QList>
#include <QMetaType>
#include <QString>
#include <sys/types.h>

namespace OS
{
    //! Describes a single Linux process
    class Process
    {
        public:
            struct Identity
            {
                pid_t   PID { 0 };
                quint64 StartTimeTicks { 0 };
            };

            struct LoadOptions
            {
                bool  IncludeKernelTasks  { true };
                bool  IncludeOtherUsers   { true };
                uid_t MyUID               { 0 };
                bool  CollectIOMetrics    { false };
                bool  IsSuperuser         { false };
                uid_t EffectiveUID        { 0 };
            };

            pid_t   PID           { 0 };
            pid_t   PPID          { 0 };
            QString Name;                     ///< Short name  (/proc/pid/comm)
            QString CmdLine;                  ///< Full command (/proc/pid/cmdline)
            char    State         { '?' };    ///< Raw state char: R S D Z T I ...
            uid_t   UID           { 0 };
            QString User;                     ///< Resolved username
            int     Priority      { 0 };
            int     Nice          { 0 };
            int     Threads       { 1 };
            quint64 VMRssKb       { 0 };      ///< Resident set size in KiB
            quint64 vmSizeKb      { 0 };      ///< Virtual memory size in KiB
            quint64 CPUTicks      { 0 };      ///< utime + stime in jiffies (for delta CPU%)
            double  CPUPercent    { 0.0 };    ///< Calculated externally after two samples
            quint64 StartTimeTicks{ 0 };      ///< Start time in jiffies since boot
            bool    IsKernelThread{ false };   ///< True when PF_KTHREAD flag is set in /proc/pid/stat flags field
            quint64 IOReadBytes   { 0 };      ///< Cumulative bytes read from storage since start.
            quint64 IOWriteBytes  { 0 };      ///< Cumulative bytes written to storage since start.
            double  IOReadBps     { 0.0 };    ///< Calculated externally after two samples.
            double  IOWriteBps    { 0.0 };    ///< Calculated externally after two samples.
            bool    IOTotalsAvailable { false }; ///< True when /proc/pid/io totals were read.
            bool    IORatesAvailable { false };  ///< True when a previous I/O sample exists.
            bool    IOPermissionDenied { false }; ///< True when I/O metrics were skipped due to permissions.

            /// Load a snapshot of every running process from /proc.
            static QList<Process> LoadAll();
            static QList<Process> LoadAll(const LoadOptions &options);

            /// Human-readable description of a raw state character.
            static QString GetStateString(char state);
            /// Identity based on PID and StartTime (should be unique) - used by perf optimizations to avoid unnecessary queries
            Identity GetIdentity() const;

        private:
            static bool loadOneStatAndUid(pid_t pid, Process &out);
            static bool loadIO(Process &proc);
            static void loadUserAndCmdline(Process &proc);
    };

    bool operator<(const Process::Identity &lhs, const Process::Identity &rhs);
    bool operator==(const Process::Identity &lhs, const Process::Identity &rhs);
} // namespace Os

Q_DECLARE_METATYPE(OS::Process)
Q_DECLARE_METATYPE(QList<OS::Process>)

#endif // OS_PROCESS_H
