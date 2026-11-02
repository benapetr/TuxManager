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

#include "cpu.h"
#include "../misc.h"
#include <QFile>
#include <QString>
#include <QDir>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/statvfs.h>
#include <utility>

CPU::CPU()
{
    this->readCpuMetadata();
    this->readHardwareMetadata();
    this->detectCpuTemperatureSensor();
}

double CPU::CorePercent(int i) const
{
    if (i < 0 || i >= static_cast<int>(this->m_cores.size()))
        return 0.0;
    const auto &c = this->m_cores.at(i);
    return c->History.IsEmpty() ? 0.0 : c->History.Back();
}

const HistoryBuffer &CPU::CoreHistory(int i) const
{
    static const HistoryBuffer empty;
    if (i < 0 || i >= static_cast<int>(this->m_cores.size()))
        return empty;
    return this->m_cores.at(i)->History;
}

const HistoryBuffer &CPU::CoreKernelHistory(int i) const
{
    static const HistoryBuffer empty;
    if (i < 0 || i >= static_cast<int>(this->m_cores.size()))
        return empty;
    return this->m_cores.at(i)->KernelHistory;
}

double CPU::CoreCurrentMhz(int i) const
{
    if (i < 0 || i >= this->m_coreCurrentMhz.size())
        return 0.0;
    return this->m_coreCurrentMhz.at(i);
}

// ── CPU sampling ──────────────────────────────────────────────────────────────

// Parse one "cpu..." line from /proc/stat — returns total jiffies and writes
// idle and kernel jiffies via output parameters.
quint64 CPU::parseCpuLine(const QList<QByteArray> &parts, quint64 &outIdle, quint64 &outKernel)
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

bool CPU::Sample()
{
    QFile f("/proc/stat");
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    int coreIdx = 0;

    for (;;)
    {
        const QByteArray raw = f.readLine();
        if (raw.isNull())
            break;

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
            this->m_cpuHistory.Push(pct);
            this->m_cpuKernelHistory.Push(kpct);
        } else if (key.startsWith("cpu") && key.size() > 3)
        {
            // Per-core line: "cpu0", "cpu1", ...
            if (coreIdx >= static_cast<int>(this->m_cores.size()))
                this->m_cores.push_back(std::make_unique<CoreSample>());

            CoreSample &c = *this->m_cores[coreIdx];
            quint64 idleC = 0, kernelC = 0;
            const quint64 totalC = parseCpuLine(parts, idleC, kernelC);

            const quint64 dTotal  = (totalC  > c.PrevTotal ) ? (totalC  - c.PrevTotal ) : 0;
            const quint64 dIdle   = (idleC   > c.PrevIdle  ) ? (idleC   - c.PrevIdle  ) : 0;
            const quint64 dKernel = (kernelC > c.PrevKernel) ? (kernelC - c.PrevKernel) : 0;

            double pct = 0.0, kpct = 0.0;
            if (dTotal > 0)
            {
                pct  = (1.0 - static_cast<double>(dIdle)   / static_cast<double>(dTotal)) * 100.0;
                kpct =        static_cast<double>(dKernel) / static_cast<double>(dTotal)  * 100.0;
            }

            c.PrevTotal  = totalC;
            c.PrevIdle   = idleC;
            c.PrevKernel = kernelC;
            c.History.Push(pct);
            c.KernelHistory.Push(kpct);
            ++coreIdx;
        } else if (coreIdx > 0)
        {
            break;   // past cpu lines — stop reading for efficiency
        }
    }

    f.close();

    this->sampleCpuTemperature();
    this->readCurrentFreq();
    return true;
}


void CPU::readCpuMetadata()
{
    this->m_cpuLogicalCount = static_cast<int>(sysconf(_SC_NPROCESSORS_ONLN));

    QFile f("/proc/cpuinfo");
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    bool gotModel = false, gotBase = false, gotFlags = false;
    bool hasHypervisorFlag = false;
    for (;;)
    {
        const QByteArray line = f.readLine();
        if (line.isNull())
            break;

        const int        colon = line.indexOf(':');
        if (colon < 0)
            continue;
        const QByteArray key = line.left(colon).trimmed();
        const QByteArray val = line.mid(colon + 1).trimmed();

        if (!gotModel && key == "model name")
        {
            this->m_cpuModelName = QString::fromUtf8(val);
            gotModel = true;
        } else if (!gotBase && key == "cpu MHz")
        {
            this->m_cpuBaseMhz = val.toDouble();
            gotBase = true;
        } else if (!gotFlags && key == "flags")
        {
            hasHypervisorFlag = val.contains("hypervisor");
            gotFlags = true;
        }

        if (gotModel && gotBase && gotFlags)
            break;
    }
    f.close();

    this->m_cpuIsVirtualMachine = hasHypervisorFlag;
    this->readCurrentFreq();
}

void CPU::detectCpuFreqSource()
{
    if (this->m_cpuFreqSourceDetected)
        return;

    this->m_cpuFreqSourceDetected = true;
    this->m_cpuFreqUseSysfs = false;
    this->m_cpuFreqPaths.clear();

    for (int i = 0; i < this->m_cpuLogicalCount; ++i)
    {
        const QString base = QString("/sys/devices/system/cpu/cpu%1/cpufreq/").arg(i);
        const QString scalingPath = base + "scaling_cur_freq";
        const QString cpuinfoPath = base + "cpuinfo_cur_freq";

        if (QFileInfo::exists(scalingPath))
            this->m_cpuFreqPaths.append(scalingPath);
        else if (QFileInfo::exists(cpuinfoPath))
            this->m_cpuFreqPaths.append(cpuinfoPath);
        else
        {
            this->m_cpuFreqPaths.clear();
            return;
        }
    }

    this->m_cpuFreqUseSysfs = !this->m_cpuFreqPaths.isEmpty();
}

void CPU::readCurrentFreq()
{
    const int coreCount = qMax(static_cast<int>(this->m_cores.size()), this->m_cpuLogicalCount);
    QVector<double> coreMhz(coreCount, 0.0);

    this->detectCpuFreqSource();

    if (this->m_cpuFreqUseSysfs)
    {
        for (int i = 0; i < this->m_cpuFreqPaths.size() && i < coreMhz.size(); ++i)
        {
            bool ok = false;
            const double kHz = Misc::ReadFile(this->m_cpuFreqPaths.at(i)).toDouble(&ok);
            if (ok && kHz > 0.0)
                coreMhz[i] = kHz / 1000.0;
        }
    } else
    {
        // Fallback: parse per-core live frequencies from /proc/cpuinfo.
        int currentProcessor = -1;
        QFile f("/proc/cpuinfo");
        if (f.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            for (;;)
            {
                const QByteArray line = f.readLine();
                if (line.isNull())
                    break;

                const int colon = line.indexOf(':');
                if (colon < 0)
                    continue;

                const QByteArray key = line.left(colon).trimmed();
                const QByteArray val = line.mid(colon + 1).trimmed();
                if (key == "processor")
                {
                    currentProcessor = val.toInt();
                }
                else if (key == "cpu MHz" && currentProcessor >= 0 && currentProcessor < coreMhz.size())
                {
                    coreMhz[currentProcessor] = val.toDouble();
                }
            }
            f.close();
        }
    }

    double sumMhz = 0.0;
    int countMhz = 0;
    for (double mhz : std::as_const(coreMhz))
    {
        if (mhz > 0.0)
        {
            sumMhz += mhz;
            ++countMhz;
        }
    }

    // Fallback when per-core values are unavailable.
    if (countMhz == 0)
    {
        QFile sf("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq");
        if (sf.open(QIODevice::ReadOnly))
        {
            const double kHz = sf.readAll().trimmed().toDouble();
            sf.close();
            if (kHz > 0.0)
            {
                const double mhz = kHz / 1000.0;
                if (!coreMhz.isEmpty())
                    coreMhz[0] = mhz;
                this->m_cpuCurrentMhz = mhz;
                this->m_coreCurrentMhz = coreMhz;
                return;
            }
        }
        this->m_cpuCurrentMhz = 0.0;
        this->m_coreCurrentMhz = coreMhz;
        return;
    }

    this->m_cpuCurrentMhz = sumMhz / static_cast<double>(countMhz);
    this->m_coreCurrentMhz = coreMhz;
}

void CPU::readHardwareMetadata()
{
    // Best-effort VM detection from DMI strings.
    const QString dmiVendor  = Misc::ReadFile("/sys/devices/virtual/dmi/id/sys_vendor").toLower();
    const QString dmiProduct = Misc::ReadFile("/sys/devices/virtual/dmi/id/product_name").toLower();
    const QString dmiBoard   = Misc::ReadFile("/sys/devices/virtual/dmi/id/board_vendor").toLower();
    const QString dmiBios    = Misc::ReadFile("/sys/devices/virtual/dmi/id/bios_vendor").toLower();
    const QString dmiAll = dmiVendor + " " + dmiProduct + " " + dmiBoard + " " + dmiBios;

    struct VmMarker { const char *needle; const char *label; };
    static const VmMarker kVmMarkers[] = {
        { "kvm",        "KVM" },
        { "qemu",       "QEMU" },
        { "vmware",     "VMware" },
        { "virtualbox", "VirtualBox" },
        { "microsoft",  "Hyper-V" },
        { "hyper-v",    "Hyper-V" },
        { "xen",        "Xen" },
        { "bhyve",      "bhyve" },
        { "parallels",  "Parallels" },
        { "apple virtualization", "Apple Virtualization" }
    };

    for (const VmMarker &m : kVmMarkers)
    {
        if (dmiAll.contains(m.needle))
        {
            this->m_cpuIsVirtualMachine = true;
            this->m_cpuVmVendor = QString::fromLatin1(m.label);
            break;
        }
    }
}

void CPU::detectCpuTemperatureSensor()
{
    this->m_cpuTempInputPath.clear();
    this->m_cpuTemperatureC = -1;

    const QDir hwmonRoot("/sys/class/hwmon");
    const QStringList hwmons = hwmonRoot.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

    struct Candidate
    {
        int score { -1 };
        QString inputPath;
    };
    Candidate best;

    static const QStringList kPreferredChipNames
        {
            "coretemp", "k10temp", "zenpower", "cpu", "soc_thermal", "x86_pkg_temp"
        };
    static const QStringList kPreferredTempLabels
        {
            "package", "tdie", "tctl", "cpu", "core"
        };

    for (const QString &hwmon : hwmons)
    {
        const QString basePath = QString("/sys/class/hwmon/%1").arg(hwmon);
        const QString chipName = Misc::ReadFile(basePath + "/name");
        const bool chipLooksCpu = Misc::TextContainsAnyToken(chipName, kPreferredChipNames);

        QDir dir(basePath);
        const QStringList tempInputs = dir.entryList(QStringList() << "temp*_input", QDir::Files, QDir::Name);
        for (const QString &input : tempInputs)
        {
            const QString idx = input.mid(4, input.size() - 10); // temp + N + _input
            const QString label = Misc::ReadFile(basePath + "/temp" + idx + "_label");
            bool ok = false;
            const int milliC = Misc::ReadFile(basePath + "/" + input).toInt(&ok);
            if (!ok || milliC <= 0)
                continue;

            int score = 0;
            if (chipLooksCpu)
                score += 20;
            if (Misc::TextContainsAnyToken(label, kPreferredTempLabels))
                score += 10;
            if (label.toLower().contains("package") || label.toLower().contains("tdie"))
                score += 10;

            // Keep plausible CPU temperatures, but allow warm systems.
            if (milliC >= 10000 && milliC <= 120000)
                score += 1;

            if (score > best.score)
            {
                best.score = score;
                best.inputPath = basePath + "/" + input;
            }
        }
    }

    if (best.score >= 0)
        this->m_cpuTempInputPath = best.inputPath;
}

void CPU::sampleCpuTemperature()
{
    if (this->m_cpuTempInputPath.isEmpty())
    {
        this->m_cpuTemperatureC = -1;
        return;
    }

    bool ok = false;
    const int milliC = Misc::ReadFile(this->m_cpuTempInputPath).toInt(&ok);
    if (!ok || milliC <= 0)
    {
        this->m_cpuTemperatureC = -1;
        return;
    }

    this->m_cpuTemperatureC = milliC / 1000;
}
