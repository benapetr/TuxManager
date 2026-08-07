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

#include "servicehelper.h"
#include "../logger.h"

#include <dlfcn.h>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>

using namespace OS;

namespace
{
    struct sd_bus;
    struct sd_bus_message;
    struct sd_bus_error;

    using FnSdBusOpenSystem = int (*)(sd_bus **);
    using FnSdBusUnref = sd_bus *(*)(sd_bus *);
    using FnSdBusCallMethod = int (*)(sd_bus *, const char *, const char *, const char *,
                                      const char *, sd_bus_error *, sd_bus_message **,
                                      const char *, ...);
    using FnSdBusMessageNewMethodCall = int (*)(sd_bus *, sd_bus_message **, const char *, const char *, const char *, const char *);
    using FnSdBusMessageAppend = int (*)(sd_bus_message *, const char *, ...);
    using FnSdBusMessageSetAllowInteractiveAuthorization = int (*)(sd_bus_message *, int);
    using FnSdBusCall = int (*)(sd_bus *, sd_bus_message *, uint64_t, sd_bus_error *, sd_bus_message **);
    using FnSdBusMessageUnref = sd_bus_message *(*)(sd_bus_message *);
    using FnSdBusMessageEnterContainer = int (*)(sd_bus_message *, char, const char *);
    using FnSdBusMessageExitContainer = int (*)(sd_bus_message *);
    using FnSdBusMessageRead = int (*)(sd_bus_message *, const char *, ...);

    void *g_sdLib = nullptr;
    FnSdBusOpenSystem pSdBusOpenSystem = nullptr;
    FnSdBusUnref pSdBusUnref = nullptr;
    FnSdBusCallMethod pSdBusCallMethod = nullptr;
    FnSdBusMessageNewMethodCall pSdBusMessageNewMethodCall = nullptr;
    FnSdBusMessageAppend pSdBusMessageAppend = nullptr;
    FnSdBusMessageSetAllowInteractiveAuthorization pSdBusMessageSetAllowInteractiveAuthorization = nullptr;
    FnSdBusCall pSdBusCall = nullptr;
    FnSdBusMessageUnref pSdBusMessageUnref = nullptr;
    FnSdBusMessageEnterContainer pSdBusMessageEnterContainer = nullptr;
    FnSdBusMessageExitContainer pSdBusMessageExitContainer = nullptr;
    FnSdBusMessageRead pSdBusMessageRead = nullptr;

    bool ensureSdBusLoaded(QString *error)
    {
        if (pSdBusOpenSystem && pSdBusCallMethod && pSdBusMessageRead)
            return true;

        if (!g_sdLib)
        {
            g_sdLib = ::dlopen("libsystemd.so.0", RTLD_LAZY | RTLD_LOCAL);
            if (!g_sdLib)
                g_sdLib = ::dlopen("libsystemd.so", RTLD_LAZY | RTLD_LOCAL);
        }
        if (!g_sdLib)
        {
            if (error)
                *error = QObject::tr("未找到 libsystemd");
            return false;
        }

        pSdBusOpenSystem = reinterpret_cast<FnSdBusOpenSystem>(::dlsym(g_sdLib, "sd_bus_open_system"));
        pSdBusUnref = reinterpret_cast<FnSdBusUnref>(::dlsym(g_sdLib, "sd_bus_unref"));
        pSdBusCallMethod = reinterpret_cast<FnSdBusCallMethod>(::dlsym(g_sdLib, "sd_bus_call_method"));
        pSdBusMessageNewMethodCall = reinterpret_cast<FnSdBusMessageNewMethodCall>(::dlsym(g_sdLib, "sd_bus_message_new_method_call"));
        pSdBusMessageAppend = reinterpret_cast<FnSdBusMessageAppend>(::dlsym(g_sdLib, "sd_bus_message_append"));
        pSdBusMessageSetAllowInteractiveAuthorization =
            reinterpret_cast<FnSdBusMessageSetAllowInteractiveAuthorization>(
                ::dlsym(g_sdLib, "sd_bus_message_set_allow_interactive_authorization"));
        pSdBusCall = reinterpret_cast<FnSdBusCall>(::dlsym(g_sdLib, "sd_bus_call"));
        pSdBusMessageUnref = reinterpret_cast<FnSdBusMessageUnref>(::dlsym(g_sdLib, "sd_bus_message_unref"));
        pSdBusMessageEnterContainer = reinterpret_cast<FnSdBusMessageEnterContainer>(::dlsym(g_sdLib, "sd_bus_message_enter_container"));
        pSdBusMessageExitContainer = reinterpret_cast<FnSdBusMessageExitContainer>(::dlsym(g_sdLib, "sd_bus_message_exit_container"));
        pSdBusMessageRead = reinterpret_cast<FnSdBusMessageRead>(::dlsym(g_sdLib, "sd_bus_message_read"));

        if (!pSdBusOpenSystem || !pSdBusUnref || !pSdBusCallMethod || !pSdBusMessageUnref
            || !pSdBusMessageEnterContainer || !pSdBusMessageExitContainer || !pSdBusMessageRead)
        {
            if (error)
                *error = QObject::tr("libsystemd 缺少 sd-bus 符号");
            return false;
        }
        if (error)
            error->clear();
        return true;
    }

    const char *managerMethodForAction(ServiceHelper::UnitAction action)
    {
        switch (action)
        {
            case ServiceHelper::UnitAction::Start:           return "StartUnit";
            case ServiceHelper::UnitAction::Stop:            return "StopUnit";
            case ServiceHelper::UnitAction::Restart:         return "RestartUnit";
            case ServiceHelper::UnitAction::Reload:          return "ReloadUnit";
            case ServiceHelper::UnitAction::TryRestart:      return "TryRestartUnit";
            case ServiceHelper::UnitAction::ReloadOrRestart: return "ReloadOrRestartUnit";
        }
        return nullptr;
    }

    QString systemctlVerbForAction(ServiceHelper::UnitAction action)
    {
        switch (action)
        {
            case ServiceHelper::UnitAction::Start:           return "start";
            case ServiceHelper::UnitAction::Stop:            return "stop";
            case ServiceHelper::UnitAction::Restart:         return "restart";
            case ServiceHelper::UnitAction::Reload:          return "reload";
            case ServiceHelper::UnitAction::TryRestart:      return "try-restart";
            case ServiceHelper::UnitAction::ReloadOrRestart: return "reload-or-restart";
        }
        return {};
    }
} // namespace

bool ServiceHelper::IsSystemdAvailable(QString *reason)
{
    const QString exe = QStandardPaths::findExecutable("systemctl");
    if (exe.isEmpty())
    {
        if (reason)
            *reason = QObject::tr("未找到 systemctl");
        return false;
    }

    if (!QFileInfo::exists("/run/systemd/system"))
    {
        if (reason)
            *reason = QObject::tr("systemd 运行时目录不存在");
        return false;
    }

    if (reason)
        reason->clear();
    return true;
}

bool ServiceHelper::RunSystemctl(const QStringList &args,
                                 QString          &stdoutText,
                                 QString          &stderrText,
                                 int              &exitCode,
                                 int               timeoutMs)
{
    stdoutText.clear();
    stderrText.clear();
    exitCode = -1;

    const QString exe = QStandardPaths::findExecutable("systemctl");
    if (exe.isEmpty())
    {
        stderrText = QObject::tr("未找到 systemctl");
        return false;
    }

    QProcess p;
    p.start(exe, args);
    if (!p.waitForStarted(500))
    {
        stderrText = QObject::tr("无法启动 systemctl");
        return false;
    }

    if (!p.waitForFinished(timeoutMs))
    {
        p.kill();
        p.waitForFinished(200);
        stderrText = QObject::tr("systemctl 超时");
        return false;
    }

    stdoutText = QString::fromUtf8(p.readAllStandardOutput());
    stderrText = QString::fromUtf8(p.readAllStandardError());
    exitCode = p.exitCode();
    return p.exitStatus() == QProcess::NormalExit;
}

bool ServiceHelper::ListServicesViaSystemdDbus(QList<ServiceRecord> &records, QString *error)
{
    records.clear();

    QString loadErr;
    if (!ensureSdBusLoaded(&loadErr))
    {
        if (error)
            *error = loadErr;
        return false;
    }

    sd_bus *bus = nullptr;
    sd_bus_message *reply = nullptr;

    int r = pSdBusOpenSystem(&bus);
    if (r < 0 || !bus)
    {
        if (error)
            *error = QObject::tr("sd-bus 打开系统失败（%1）").arg(r);
        return false;
    }

    r = pSdBusCallMethod(bus,
                         "org.freedesktop.systemd1",
                         "/org/freedesktop/systemd1",
                         "org.freedesktop.systemd1.Manager",
                         "ListUnits",
                         nullptr,
                         &reply,
                         nullptr);
    if (r < 0 || !reply)
    {
        if (error)
            *error = QObject::tr("sd-bus ListUnits 调用失败（%1）").arg(r);
        if (reply)
            pSdBusMessageUnref(reply);
        pSdBusUnref(bus);
        return false;
    }

    r = pSdBusMessageEnterContainer(reply, 'a', "(ssssssouso)");
    if (r < 0)
    {
        if (error)
            *error = QObject::tr("sd-bus 解码失败（%1）").arg(r);
        pSdBusMessageUnref(reply);
        pSdBusUnref(bus);
        return false;
    }

    while ((r = pSdBusMessageEnterContainer(reply, 'r', "ssssssouso")) > 0)
    {
        const char *unit = nullptr;
        const char *description = nullptr;
        const char *loadState = nullptr;
        const char *activeState = nullptr;
        const char *subState = nullptr;
        const char *following = nullptr;
        const char *objectPath = nullptr;
        unsigned int jobId = 0;
        const char *jobType = nullptr;
        const char *jobPath = nullptr;

        const int rr = pSdBusMessageRead(reply, "ssssssouso",
                                         &unit,
                                         &description,
                                         &loadState,
                                         &activeState,
                                         &subState,
                                         &following,
                                         &objectPath,
                                         &jobId,
                                         &jobType,
                                         &jobPath);
        const int er = pSdBusMessageExitContainer(reply); // row
        Q_UNUSED(following);
        Q_UNUSED(objectPath);
        Q_UNUSED(jobId);
        Q_UNUSED(jobType);
        Q_UNUSED(jobPath);

        if (rr < 0 || er < 0)
        {
            if (error)
                *error = QObject::tr("sd-bus 读取行失败（%1/%2）").arg(rr).arg(er);
            pSdBusMessageExitContainer(reply); // array (best effort)
            pSdBusMessageUnref(reply);
            pSdBusUnref(bus);
            return false;
        }

        const QString unitName = QString::fromLatin1(unit ? unit : "");
        if (!unitName.endsWith(".service"))
            continue;

        ServiceRecord rec;
        rec.unit = unitName;
        rec.description = QString::fromLatin1(description ? description : "");
        rec.loadState = QString::fromLatin1(loadState ? loadState : "");
        rec.activeState = QString::fromLatin1(activeState ? activeState : "");
        rec.subState = QString::fromLatin1(subState ? subState : "");
        records.append(rec);
    }

    pSdBusMessageExitContainer(reply); // array
    pSdBusMessageUnref(reply);
    pSdBusUnref(bus);

    if (r < 0)
    {
        if (error)
            *error = QObject::tr("sd-bus 迭代失败（%1）").arg(r);
        return false;
    }

    if (error)
        error->clear();
    return true;
}

bool ServiceHelper::ManageUnit(const QString &unit, UnitAction action, QString *error)
{
    if (error)
        error->clear();

    QString reason;
    if (!IsSystemdAvailable(&reason))
    {
        if (error)
            *error = reason;
        return false;
    }

    const char *method = managerMethodForAction(action);
    if (!method || unit.isEmpty())
    {
        if (error)
            *error = QObject::tr("无效的服务操作");
        return false;
    }

    LOG_DEBUG(QString("Managing service %1 via preferred sd-bus method %2").arg(unit, QString::fromLatin1(method)));

    QString loadErr;
    if (ensureSdBusLoaded(&loadErr))
    {
        sd_bus *bus = nullptr;
        sd_bus_message *message = nullptr;
        sd_bus_message *reply = nullptr;
        const int openResult = pSdBusOpenSystem(&bus);
        if (openResult >= 0 && bus)
        {
            int callResult = -1;
            if (pSdBusMessageNewMethodCall && pSdBusMessageAppend && pSdBusMessageSetAllowInteractiveAuthorization && pSdBusCall)
            {
                const QByteArray unitUtf8 = unit.toUtf8();
                const int newMsgResult = pSdBusMessageNewMethodCall(bus,
                                                                    &message,
                                                                    "org.freedesktop.systemd1",
                                                                    "/org/freedesktop/systemd1",
                                                                    "org.freedesktop.systemd1.Manager",
                                                                    method);
                if (newMsgResult >= 0 && message)
                {
                    const int appendResult = pSdBusMessageAppend(message, "ss", unitUtf8.constData(), "replace");
                    const int authResult = (appendResult >= 0)
                                           ? pSdBusMessageSetAllowInteractiveAuthorization(message, 1)
                                           : appendResult;
                    callResult = (authResult >= 0)
                                 ? pSdBusCall(bus, message, 0, nullptr, &reply)
                                 : authResult;
                } else
                {
                    callResult = newMsgResult;
                }
            } else
            {
                if (error)
                    *error = QObject::tr("libsystemd 缺少交互式 sd-bus 符号");
                LOG_DEBUG(QString("Interactive sd-bus symbols unavailable for %1, falling back to systemctl").arg(unit));
            }

            if (message)
                pSdBusMessageUnref(message);
            if (reply)
                pSdBusMessageUnref(reply);
            pSdBusUnref(bus);

            if (callResult >= 0)
            {
                LOG_DEBUG(QString("Managed service %1 via sd-bus method %2").arg(unit, QString::fromLatin1(method)));
                return true;
            }

            if (error)
                *error = QObject::tr("systemd D-Bus 调用 %1（针对 %2）失败（%3）").arg(QString::fromLatin1(method), unit, QString::number(callResult));
            LOG_DEBUG(QString("sd-bus management for %1 failed, falling back to systemctl: %2").arg(unit, error ? *error : QString()));
        } else if (error)
        {
            *error = QObject::tr("sd-bus 打开系统失败（%1）").arg(openResult);
            LOG_DEBUG(QString("sd-bus open for managing %1 failed, falling back to systemctl: %2").arg(unit, *error));
        }
    } else
    {
        LOG_DEBUG(QString("sd-bus unavailable for managing %1, falling back to systemctl: %2").arg(unit, loadErr));
    }

    QString stdoutText;
    QString stderrText;
    int exitCode = -1;
    const QString verb = systemctlVerbForAction(action);
    const QStringList args { verb, unit, "--no-pager" };
    LOG_DEBUG(QString("Managing service %1 via systemctl fallback: %2").arg(unit, args.join(' ')));
    if (!RunSystemctl(args, stdoutText, stderrText, exitCode, kSystemctlManageTimeoutMs) || exitCode != 0)
    {
        if (error)
        {
            *error = stderrText.trimmed();
            if (error->isEmpty())
                *error = QObject::tr("systemctl %1 %2 失败").arg(verb, unit);
        }
        LOG_DEBUG(QString("systemctl fallback failed for %1: exit=%2 stderr=%3").arg(unit, QString::number(exitCode), stderrText.trimmed()));
        return false;
    }

    LOG_DEBUG(QString("Managed service %1 via systemctl fallback").arg(unit));
    return true;
}
