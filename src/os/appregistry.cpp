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

#include "appregistry.h"

#include "../logger.h"

#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QStyle>
#include <QTimer>

using namespace OS;

namespace
{
    const QString GENERIC_ICON_NAME = QStringLiteral("application-x-executable");
    constexpr int RESCAN_DELAY_MS = 2000;
    constexpr int MAX_ANCESTOR_DEPTH = 64;

    // Adapted from https://gitlab.com/mission-center-devs/app-detection

    // Programs that are only a vehicle for the real binary named later on the Exec= line.
    const QSet<QString> &transparentLaunchers()
    {
        static const QSet<QString> s_set = {
            "env", "distrobox", "distrobox-enter", "toolbox", "toolbox-enter", "toolbx", "toolbx-enter"};
        return s_set;
    }

    // Sandboxed launchers whose target binary is not visible on the host filesystem.
    const QSet<QString> &opaqueLaunchers()
    {
        static const QSet<QString> s_set = {"flatpak", "snap"};
        return s_set;
    }

    // Interpreters and coreutils. A desktop entry whose Exec= starts with one of these
    // would otherwise claim every process running that interpreter.
    const QSet<QString> &interpreters()
    {
        static const QSet<QString> s_set = {
            "arch", "ash", "awk", "base32", "base64", "basename", "basenc", "bash", "cat", "chcon",
            "chgrp", "chmod", "chown", "chroot", "cksum", "comm", "cp", "csplit", "cut", "dash",
            "date", "dd", "df", "dircolors", "dirname", "dotnet", "du", "echo", "expand", "expr",
            "factor", "false", "fish", "fmt", "fold", "groups", "hashsum", "head", "hostid",
            "hostname", "id", "install", "java", "join", "kill", "link", "ln", "logname", "ls",
            "lua", "md5sum", "mkdir", "mkfifo", "mknod", "mktemp", "more", "mv", "nice", "nl",
            "node", "nodejs", "nohup", "nproc", "numfmt", "od", "paste", "pathchk", "perl", "php",
            "pinky", "powershell", "pr", "printenv", "printf", "ptx", "pwd", "python", "python2",
            "python2.7", "python3", "readlink", "realpath", "relpath", "rm", "rmdir", "ruby",
            "runcon", "seq", "sh", "sha1sum", "sha224sum", "sha256sum", "sha384sum", "sha512sum",
            "shred", "shuf", "sleep", "sort", "split", "stat", "stdbuf", "stty", "sum", "sync",
            "tac", "tail", "tee", "test", "timeout", "touch", "tr", "true", "truncate", "tsort",
            "tty", "uname", "unexpand", "uniq", "unlink", "uptime", "users", "wc", "who", "whoami",
            "yes", "zsh"};
        return s_set;
    }

    // Binaries whose name differs from the one advertised by their desktop entry.
    const QHash<QString, QString> &executableExceptions()
    {
        static const QHash<QString, QString> s_map = {
            {"firefox-bin", "firefox"},
            {"oosplash", "libreoffice"},
            {"soffice.bin", "libreoffice"},
            {"chrome", "google-chrome-stable"},
            {"gnome-terminal-server", "gnome-terminal"}};
        return s_map;
    }

    // Unit naming schemes from https://systemd.io/DESKTOP_ENVIRONMENTS/ and common variants.
    // Pattern and unescaping adapted from libksysguard processcore/cgroup.cpp (LGPL-2.0-or-later).
    //   app[-<launcher>]-<ApplicationID>-<RANDOM>.scope
    //   app[-<launcher>]-<ApplicationID>[@<RANDOM>].service
    //   app[-<launcher>]-<ApplicationID>[@<RANDOM>].slice   (GNOME Terminal)
    //   apps-...  (historical prefix), flatpak-..., dbus-...
    const QRegularExpression &appUnitPattern()
    {
        static const QRegularExpression s_re(
            QStringLiteral("^(apps|app|flatpak|dbus)-(?:[^-]*-)?([^-]+(?=-.*\\.scope)|[^@]+(?=(?:@.*)?\\.(?:service|slice)))"));
        return s_re;
    }

    // Snap units do not follow the specification:
    //   snap.<snap>.<app>-<UUID>.scope   (applications)
    //   snap.<snap>.<app>.service        (daemons)
    // The desktop entry installed by snapd is named <snap>_<app>.desktop.
    const QRegularExpression &snapUnitPattern()
    {
        static const QRegularExpression s_re(
            QStringLiteral("^snap\\.([^.]+)\\.(.+?)(?:-[0-9a-f]{8}(?:-[0-9a-f]{4}){3}-[0-9a-f]{12})?\\.(?:scope|service)$"));
        return s_re;
    }

    // Executables inside a snap mount: /snap/<snap>/<rev>/... or /var/lib/snapd/snap/<snap>/<rev>/...
    const QRegularExpression &snapExePattern()
    {
        static const QRegularExpression s_re(QStringLiteral("^(?:/var/lib/snapd)?/snap/([^/]+)/"));
        return s_re;
    }

    // Splits an Exec= line on whitespace, honouring double quotes.
    QStringList tokenizeExec(const QString &exec)
    {
        QStringList tokens;
        QString current;
        bool quoted = false;
        for (const QChar c : exec)
        {
            if (c == '"')
            {
                quoted = !quoted;
                continue;
            }
            if (c.isSpace() && !quoted)
            {
                if (!current.isEmpty())
                {
                    tokens.append(current);
                    current.clear();
                }
                continue;
            }
            current.append(c);
        }
        if (!current.isEmpty())
            tokens.append(current);
        return tokens;
    }

    QString findPixmap(const QString &name)
    {
        static const QStringList extensions = {".png", ".svg", ".xpm"};
        for (const QString &dir : QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, "pixmaps", QStandardPaths::LocateDirectory))
        {
            for (const QString &ext : extensions)
            {
                const QString candidate = dir + '/' + name + ext;
                if (QFile::exists(candidate))
                    return candidate;
            }
        }
        return QString();
    }

    // Flatpak mounts /.flatpak-info inside every sandbox with the application id. Reading it
    // through /proc/<pid>/root works for processes of the same user and is the only way to
    // identify a flatpak application when no systemd unit exists.
    QString flatpakAppId(pid_t pid)
    {
        QFile file(QString("/proc/%1/root/.flatpak-info").arg(pid));
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
            return QString();

        bool in_group = false;
        for (;;)
        {
            const QByteArray raw = file.readLine();
            if (raw.isNull())
                break;
            const QString line = QString::fromUtf8(raw).trimmed();
            if (line.startsWith('['))
            {
                in_group = (line == QLatin1String("[Application]"));
                continue;
            }
            if (in_group && line.startsWith(QLatin1String("name=")))
                return line.mid(5);
        }
        return QString();
    }

    QString canonicalExecutable(const QString &path)
    {
        const QFileInfo info(path);
        if (!info.exists())
            return QString();
        return info.canonicalFilePath();
    }
}

AppRegistry::AppRegistry(QObject *parent) : QObject(parent)
{
    this->m_watcher = new QFileSystemWatcher(this);
    this->m_rescanTimer = new QTimer(this);
    this->m_rescanTimer->setSingleShot(true);
    this->m_rescanTimer->setInterval(RESCAN_DELAY_MS);

    connect(this->m_watcher, &QFileSystemWatcher::directoryChanged, this->m_rescanTimer, qOverload<>(&QTimer::start));
    connect(this->m_rescanTimer, &QTimer::timeout, this, &AppRegistry::Invalidate);
}

void AppRegistry::Annotate(QList<Process> &processes)
{
    this->ensureIndexed();

    QHash<pid_t, int> index_by_pid;
    QSet<Process::Identity> live;
    index_by_pid.reserve(processes.size());
    live.reserve(processes.size());

    for (int i = 0; i < processes.size(); ++i)
    {
        Process &proc = processes[i];
        index_by_pid.insert(proc.PID, i);
        proc.IconName.clear();
        if (proc.IsKernelThread)
            continue;

        const Process::Identity identity = proc.GetIdentity();
        live.insert(identity);

        auto it = this->m_resolved.constFind(identity);
        if (it == this->m_resolved.cend())
            it = this->m_resolved.insert(identity, this->resolveOwn(proc));
        proc.IconName = it.value();
    }

    for (Process &proc : processes)
    {
        if (proc.IsKernelThread || !proc.IconName.isEmpty())
            continue;

        pid_t ppid = proc.PPID;
        for (int depth = 0; depth < MAX_ANCESTOR_DEPTH && ppid > 0; ++depth)
        {
            const auto it = index_by_pid.constFind(ppid);
            if (it == index_by_pid.cend())
                break;
            const Process &ancestor = processes.at(it.value());
            if (!ancestor.IconName.isEmpty())
            {
                proc.IconName = ancestor.IconName;
                break;
            }
            ppid = ancestor.PPID;
        }
    }

    for (auto it = this->m_resolved.begin(); it != this->m_resolved.end();)
    {
        if (live.contains(it.key()))
            ++it;
        else
            it = this->m_resolved.erase(it);
    }
}

QIcon AppRegistry::IconFor(const QString &icon_name)
{
    const QString key = icon_name.isEmpty() ? GENERIC_ICON_NAME : icon_name;

    const auto it = this->m_icons.constFind(key);
    if (it != this->m_icons.cend())
        return it.value();

    QIcon icon;
    if (key.startsWith('/'))
    {
        if (QFile::exists(key))
            icon = QIcon(key);
    } else if (QIcon::hasThemeIcon(key))
    {
        icon = QIcon::fromTheme(key);
    } else
    {
        // Some packages install their icon only into a legacy pixmaps directory, which the
        // icon theme lookup does not cover.
        const QString pixmap = findPixmap(key);
        if (!pixmap.isEmpty())
            icon = QIcon(pixmap);
    }

    if (icon.isNull() && key != GENERIC_ICON_NAME)
        icon = this->IconFor(QString());

    // Without a complete icon theme the generic name is missing as well; fall back to the
    // widget style so every row still gets an icon and names stay aligned.
    if (icon.isNull() && key == GENERIC_ICON_NAME && QApplication::style())
        icon = QApplication::style()->standardIcon(QStyle::SP_FileIcon);

    this->m_icons.insert(key, icon);
    return icon;
}

void AppRegistry::Invalidate()
{
    this->m_indexed = false;
    this->m_byId.clear();
    this->m_byExec.clear();
    this->m_byExecBasename.clear();
    this->m_bySnap.clear();
    this->m_resolved.clear();
    this->m_icons.clear();
}

QString AppRegistry::AppIdFromCGroup(const QString &cgroup_path)
{
    const QStringList parts = cgroup_path.split('/', Qt::SkipEmptyParts);
    for (auto it = parts.crbegin(); it != parts.crend(); ++it)
    {
        QRegularExpressionMatch match = appUnitPattern().match(*it);
        if (match.hasMatch())
            return unescapeUnitName(match.captured(2));

        match = snapUnitPattern().match(*it);
        if (match.hasMatch())
            return match.captured(1) + '_' + match.captured(2);
    }
    return QString();
}

void AppRegistry::ensureIndexed()
{
    if (!this->m_indexed)
        this->buildIndex();
}

void AppRegistry::buildIndex()
{
    QElapsedTimer timer;
    timer.start();

    const QStringList dirs = this->applicationDirs();

    const QStringList watched = this->m_watcher->directories();
    if (!watched.isEmpty())
        this->m_watcher->removePaths(watched);

    for (const QString &dir : dirs)
    {
        if (!QDir(dir).exists())
            continue;
        this->indexDirectory(dir);
        this->m_watcher->addPath(dir);
    }

    // Flatpak exports are not always part of XDG_DATA_DIRS; make their icons resolvable.
    QStringList theme_paths = QIcon::themeSearchPaths();
    bool theme_paths_changed = false;
    for (const QString &dir : dirs)
    {
        if (!dir.contains(QLatin1String("/flatpak/exports/")))
            continue;
        const QString icon_dir = QFileInfo(dir).path() + QLatin1String("/icons");
        if (QDir(icon_dir).exists() && !theme_paths.contains(icon_dir))
        {
            theme_paths.append(icon_dir);
            theme_paths_changed = true;
        }
    }
    if (theme_paths_changed)
        QIcon::setThemeSearchPaths(theme_paths);

    this->m_indexed = true;
    LOG_DEBUG(QString("AppRegistry: indexed %1 desktop entries (%2 unique executables) in %3 ms")
                  .arg(this->m_byId.size())
                  .arg(this->m_byExec.size())
                  .arg(timer.elapsed()));
}

QStringList AppRegistry::applicationDirs() const
{
    QStringList data_dirs;

    QString data_home = qEnvironmentVariable("XDG_DATA_HOME");
    if (data_home.isEmpty())
        data_home = QDir::homePath() + QLatin1String("/.local/share");
    data_dirs.append(data_home);

    const QString xdg_data_dirs = qEnvironmentVariable("XDG_DATA_DIRS");
    if (xdg_data_dirs.isEmpty())
        data_dirs << QStringLiteral("/usr/local/share") << QStringLiteral("/usr/share");
    else
        data_dirs.append(xdg_data_dirs.split(':', Qt::SkipEmptyParts));

    data_dirs.append(data_home + QLatin1String("/flatpak/exports/share"));
    data_dirs.append(QStringLiteral("/var/lib/flatpak/exports/share"));
    data_dirs.append(QStringLiteral("/var/lib/snapd/desktop"));

    QStringList result;
    for (const QString &dir : data_dirs)
    {
        const QString app_dir = QDir::cleanPath(dir) + QLatin1String("/applications");
        if (!result.contains(app_dir))
            result.append(app_dir);
    }
    return result;
}

void AppRegistry::indexDirectory(const QString &dir)
{
    const QFileInfoList files = QDir(dir).entryInfoList({QStringLiteral("*.desktop")}, QDir::Files | QDir::Readable);
    for (const QFileInfo &file : files)
    {
        DesktopEntry entry;
        entry.Id = file.completeBaseName();
        if (this->m_byId.contains(entry.Id))
            continue;
        if (!this->parseDesktopFile(file.filePath(), entry))
            continue;

        this->m_byId.insert(entry.Id, entry);

        // snapd names entries <snap>_<app>; prefer the main app (<snap>_<snap>) for the package.
        if (dir.startsWith(QLatin1String("/var/lib/snapd/")))
        {
            const QString snap_name = entry.Id.section('_', 0, 0);
            const QString main_id = snap_name + '_' + snap_name;
            if (entry.Id == main_id || !this->m_bySnap.contains(snap_name))
                this->m_bySnap.insert(snap_name, entry.Id);
        }

        if (entry.ExecPath.isEmpty())
            continue;

        // Several entries may launch the same binary. Keep the match when they agree on
        // the icon, otherwise mark the binary as ambiguous with an empty id.
        const auto exec_it = this->m_byExec.constFind(entry.ExecPath);
        if (exec_it == this->m_byExec.cend())
            this->m_byExec.insert(entry.ExecPath, entry.Id);
        else if (!exec_it.value().isEmpty() && this->m_byId.value(exec_it.value()).Icon != entry.Icon)
            this->m_byExec.insert(entry.ExecPath, QString());

        const QString basename = QFileInfo(entry.ExecPath).fileName();
        if (!this->m_byExecBasename.contains(basename))
            this->m_byExecBasename.insert(basename, entry.Id);
    }
}

bool AppRegistry::parseDesktopFile(const QString &path, DesktopEntry &out) const
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    bool in_group = false;
    QString exec;
    for (;;)
    {
        const QByteArray raw = file.readLine();
        if (raw.isNull())
            break;
        const QString line = QString::fromUtf8(raw).trimmed();

        if (line.startsWith('['))
        {
            if (in_group)
                break;
            in_group = (line == QLatin1String("[Desktop Entry]"));
            continue;
        }
        if (!in_group || line.isEmpty() || line.startsWith('#'))
            continue;

        const int eq = line.indexOf('=');
        if (eq <= 0)
            continue;
        const QString key = line.left(eq).trimmed();
        const QString value = line.mid(eq + 1).trimmed();

        if (key == QLatin1String("Type"))
        {
            if (value != QLatin1String("Application"))
                return false;
        } else if (key == QLatin1String("Hidden"))
        {
            if (value == QLatin1String("true"))
                return false;
        } else if (key == QLatin1String("Name"))
        {
            out.Name = value;
        } else if (key == QLatin1String("Icon"))
        {
            out.Icon = value;
        } else if (key == QLatin1String("Exec"))
        {
            exec = value;
        }
    }

    if (out.Icon.isEmpty())
        return false;

    out.ExecPath = this->resolveExec(exec);
    return true;
}

QString AppRegistry::resolveExec(const QString &exec) const
{
    const QStringList tokens = tokenizeExec(exec);
    for (const QString &token : tokens)
    {
        if (token.startsWith('-') || token.startsWith('%'))
            continue;

        QString resolved;
        if (token.startsWith('/'))
            resolved = canonicalExecutable(token);
        else
            resolved = canonicalExecutable(QStandardPaths::findExecutable(token));

        if (resolved.isEmpty())
            continue;

        const QString basename = QFileInfo(resolved).fileName();
        if (opaqueLaunchers().contains(basename) || interpreters().contains(basename))
            return QString();
        if (transparentLaunchers().contains(basename))
            continue;

        return resolved;
    }
    return QString();
}

QString AppRegistry::resolveOwn(const Process &proc) const
{
    if (!proc.CGroup.isEmpty())
    {
        const QString app_id = AppIdFromCGroup(proc.CGroup);
        if (!app_id.isEmpty())
        {
            const auto it = this->m_byId.constFind(app_id);
            if (it != this->m_byId.cend())
                return it.value().Icon;
        }
    }

    if (proc.ExePath.startsWith(QLatin1String("/app/")) || proc.ExePath.startsWith(QLatin1String("/usr/")))
    {
        const QString app_id = flatpakAppId(proc.PID);
        if (!app_id.isEmpty())
        {
            const auto it = this->m_byId.constFind(app_id);
            if (it != this->m_byId.cend())
                return it.value().Icon;
        }
    }

    QString exe = proc.ExePath;
    if (exe.isEmpty())
        exe = canonicalExecutable(QStandardPaths::findExecutable(proc.Name));

    if (!exe.isEmpty())
    {
        const QRegularExpressionMatch snap_match = snapExePattern().match(exe);
        if (snap_match.hasMatch())
        {
            const QString by_snap = this->m_bySnap.value(snap_match.captured(1));
            if (!by_snap.isEmpty())
                return this->m_byId.value(by_snap).Icon;
        }

        const QString by_exec = this->m_byExec.value(exe);
        if (!by_exec.isEmpty())
            return this->m_byId.value(by_exec).Icon;

        const QString replacement = executableExceptions().value(QFileInfo(exe).fileName());
        if (!replacement.isEmpty())
        {
            const QString by_basename = this->m_byExecBasename.value(replacement);
            if (!by_basename.isEmpty())
                return this->m_byId.value(by_basename).Icon;
        }
    }

    const QString theme_name = proc.Name.section(' ', 0, 0).toLower();
    if (!theme_name.isEmpty() && QIcon::hasThemeIcon(theme_name))
        return theme_name;

    return QString();
}

QString AppRegistry::unescapeUnitName(const QString &name)
{
    // systemd escapes bytes outside its safe set as \xZZ.
    QByteArray bytes;
    bytes.reserve(name.size());
    const QByteArray raw = name.toUtf8();
    for (int i = 0; i < raw.size(); ++i)
    {
        if (raw[i] == '\\' && i + 3 < raw.size() && raw[i + 1] == 'x')
        {
            bool ok = false;
            const int value = raw.mid(i + 2, 2).toInt(&ok, 16);
            if (ok)
            {
                bytes.append(static_cast<char>(value));
                i += 3;
                continue;
            }
        }
        bytes.append(raw[i]);
    }
    return QString::fromUtf8(bytes);
}
