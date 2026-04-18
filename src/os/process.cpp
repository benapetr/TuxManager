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

#include "process.h"

#include <QDir>
#include <QFile>
#include <QHash>

#include <pwd.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace OS;

// ── Helpers ──────────────────────────────────────────────────────────────────

namespace
{
    //! Calls sysconf once and caches the result - this can't change in runtime
    long cachedPageSize()
    {
        static long s_pageSize = sysconf(_SC_PAGESIZE);
        if (s_pageSize <= 0)
            s_pageSize = 4096L;
        return s_pageSize;
    }

    QString cachedUserNameForUid(uid_t uid)
    {
        static QHash<uid_t, QString> s_userNameByUid;

        const auto it = s_userNameByUid.constFind(uid);
        if (it != s_userNameByUid.cend())
            return it.value();

        const struct passwd *pw = getpwuid(uid);
        const QString userName = pw ? QString::fromUtf8(pw->pw_name)
                                    : QString::number(uid);
        s_userNameByUid.insert(uid, userName);
        return userName;
    }
}

QString Process::GetStateString(char state)
{
    switch (state)
    {
        case 'R': return "Running";
        case 'S': return "Sleeping";
        case 'D': return "Disk sleep";
        case 'Z': return "Zombie";
        case 'T': return "Stopped";
        case 't': return "Tracing stop";
        case 'I': return "Idle";
        case 'X': return "Dead";
        default:  return QString(state);
    }
}

Process::Identity Process::GetIdentity() const
{
    return { this->PID, this->StartTimeTicks };
}

bool OS::operator<(const Process::Identity &lhs, const Process::Identity &rhs)
{
    if (lhs.PID != rhs.PID)
        return lhs.PID < rhs.PID;
    return lhs.StartTimeTicks < rhs.StartTimeTicks;
}

bool OS::operator==(const Process::Identity &lhs, const Process::Identity &rhs)
{
    return lhs.PID == rhs.PID && lhs.StartTimeTicks == rhs.StartTimeTicks;
}

// ── Private: load a single process ───────────────────────────────────────────

bool Process::loadOneStatAndUid(pid_t pid, Process &out)
{
    out.PID = pid;

    // ── /proc/pid/stat ──────────────────────────────────────────────────────
    // Format: pid (comm) state ppid pgrp session tty_nr tpgid flags
    //         minflt cminflt majflt cmajflt utime stime cutime cstime
    //         priority nice num_threads itrealvalue starttime vsize rss ...
    //
    // comm can contain spaces and parentheses, so we anchor on the last ')'.

    QFile statFile(QString("/proc/%1/stat").arg(pid));
    if (!statFile.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    const QByteArray statData = statFile.readAll().trimmed();
    statFile.close();

    const int commStart = statData.indexOf('(');
    const int commEnd   = statData.lastIndexOf(')');
    if (commStart < 0 || commEnd < 0 || commEnd <= commStart)
        return false;

    out.Name = QString::fromUtf8(statData.mid(commStart + 1, commEnd - commStart - 1));

    // Fields after ") " (0-indexed):
    //  0:state 1:ppid 2:pgrp 3:session 4:tty_nr 5:tpgid 6:flags
    //  7:minflt 8:cminflt 9:majflt 10:cmajflt
    //  11:utime 12:stime 13:cutime 14:cstime
    //  15:priority 16:nice 17:num_threads 18:itrealvalue
    //  19:starttime 20:vsize 21:rss
    const QList<QByteArray> f = statData.mid(commEnd + 2).split(' ');
    if (f.size() < 22)
        return false;

    // PF_KTHREAD is the authoritative kernel-thread flag (same as htop)
    static constexpr quint32 PF_KTHREAD = 0x00200000;
    const quint32 procFlags = f[6].toUInt();
    out.IsKernelThread = (procFlags & PF_KTHREAD) != 0;

    out.State          = f[0].isEmpty() ? '?' : f[0].at(0);
    out.PPID           = f[1].toLong();
    out.CPUTicks       = f[11].toULongLong() + f[12].toULongLong(); // utime + stime
    out.Priority       = f[15].toInt();
    out.Nice           = f[16].toInt();
    out.Threads        = f[17].toInt();
    out.StartTimeTicks = f[19].toULongLong();
    out.vmSizeKb       = f[20].toULongLong() / 1024ULL;

    const long pageSize = cachedPageSize();
    out.VMRssKb        = static_cast<quint64>(f[21].toLongLong()) * static_cast<quint64>(pageSize) / 1024ULL;

    // ── UID via stat() on /proc/pid directory ─────────────────────────────────
    // The /proc/<pid> directory is owned by the process's real UID — far more
    // reliable than parsing /proc/<pid>/status which may use spaces or tabs.
    {
        struct stat st{};
        if (::stat(QString("/proc/%1").arg(pid).toLocal8Bit().constData(), &st) == 0)
            out.UID = st.st_uid;
    }

    return true;
}

bool Process::loadIO(Process &proc)
{
    QFile ioFile(QString("/proc/%1/io").arg(proc.PID));
    if (!ioFile.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    quint64 readBytes = 0;
    quint64 writeBytes = 0;
    bool haveReadBytes = false;
    bool haveWriteBytes = false;

    for (;;)
    {
        const QByteArray line = ioFile.readLine();
        if (line.isNull())
            break;

        const QByteArray trimmedLine = line.trimmed();
        const int colonPos = trimmedLine.indexOf(':');
        if (colonPos <= 0)
            continue;

        const QByteArray key = trimmedLine.left(colonPos);
        const QByteArray value = trimmedLine.mid(colonPos + 1).trimmed();
        if (key == "read_bytes")
        {
            readBytes = value.toULongLong();
            haveReadBytes = true;
        } else if (key == "write_bytes")
        {
            writeBytes = value.toULongLong();
            haveWriteBytes = true;
        }
    }

    ioFile.close();

    if (!haveReadBytes || !haveWriteBytes)
        return false;

    proc.IOReadBytes = readBytes;
    proc.IOWriteBytes = writeBytes;
    proc.IOTotalsAvailable = true;
    proc.IOPermissionDenied = false;
    return true;
}

void Process::loadUserAndCmdline(Process &proc)
{
    // Cache UID -> username/fallback string to avoid repeated getpwuid() lookups.
    proc.User = cachedUserNameForUid(proc.UID);

    // ── /proc/pid/cmdline ────────────────────────────────────────────────────
    // Kernel threads have no cmdline; use bracketed name as display string.
    if (proc.IsKernelThread)
    {
        proc.CmdLine = "[" + proc.Name + "]";
        return;
    }

    QFile cmdFile(QString("/proc/%1/cmdline").arg(proc.PID));
    if (cmdFile.open(QIODevice::ReadOnly))
    {
        QByteArray data = cmdFile.readAll();
        cmdFile.close();
        data.replace('\0', ' ');
        proc.CmdLine = QString::fromUtf8(data).trimmed();
    }
    if (proc.CmdLine.isEmpty())
        proc.CmdLine = proc.Name; // fallback: use comm name
}

// ── Public: load all processes ────────────────────────────────────────────────

QList<Process> Process::LoadAll()
{
    return Process::LoadAll(LoadOptions{});
}

QList<Process> Process::LoadAll(const LoadOptions &options)
{
    QList<Process> list;

    const QDir procDir("/proc");
    const QStringList entries = procDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

    list.reserve(entries.size());
    for (const QString &entry : entries)
    {
        bool ok;
        const pid_t pid = static_cast<pid_t>(entry.toInt(&ok));
        if (!ok)
            continue;

        Process proc;
        if (!loadOneStatAndUid(pid, proc))
            continue;

        // Fast prefilter on fields available from stat + proc dir ownership.
        if (!options.IncludeKernelTasks && proc.IsKernelThread)
            continue;
        if (!options.IncludeOtherUsers && proc.UID != options.MyUID)
            continue;

        if (options.CollectIOMetrics)
        {
            if (options.IsSuperuser || proc.UID == options.EffectiveUID)
            {
                if (!loadIO(proc))
                    proc.IOPermissionDenied = true;
            } else
            {
                proc.IOPermissionDenied = true;
            }
        }

        loadUserAndCmdline(proc);
        list.append(proc);
    }

    return list;
}
