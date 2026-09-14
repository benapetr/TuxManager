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

#ifndef OS_APPREGISTRY_H
#define OS_APPREGISTRY_H

#include "process.h"

#include <QHash>
#include <QIcon>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

class QFileSystemWatcher;
class QTimer;

namespace OS
{
    /// Maps running processes to desktop applications and their icons.
    ///
    /// Data sources, in order of precedence:
    ///   1. systemd/XDG cgroup unit name (https://systemd.io/DESKTOP_ENVIRONMENTS/)
    ///   2. canonical executable path matched against Exec= of installed desktop entries
    ///   3. icon theme entry named after the process
    ///   4. icon of the nearest ancestor process that resolved
    ///
    /// Desktop entries are indexed from $XDG_DATA_HOME and $XDG_DATA_DIRS and re-indexed
    /// when any applications directory changes. Resolution results are cached per process
    /// identity (PID + start time), so each process is resolved once during its lifetime.
    class AppRegistry : public QObject
    {
        Q_OBJECT

        public:
            explicit AppRegistry(QObject *parent = nullptr);

            /// Fills Process::IconName for every process in the snapshot.
            void Annotate(QList<Process> &processes);

            /// Returns the icon for a name produced by Annotate. Empty or unknown names
            /// yield the generic executable icon so rows stay aligned.
            QIcon IconFor(const QString &icon_name);

            /// Drops the desktop index and all caches; the index is rebuilt on next use.
            void Invalidate();

            /// Extracts the application id from a cgroup path, or an empty string.
            /// Exposed for documentation of the unit naming scheme; see docs/process-icons.md.
            static QString AppIdFromCGroup(const QString &cgroup_path);

        private:
            struct DesktopEntry
            {
                QString Id;        ///< Desktop file name without the .desktop suffix
                QString Name;
                QString Icon;      ///< Icon= value verbatim: theme name or absolute path
                QString ExecPath;  ///< Canonical path of the primary binary, empty when not resolvable
            };

            bool                                  m_indexed { false };
            QHash<QString, DesktopEntry>          m_byId;
            QHash<QString, QString>               m_byExec;    ///< Canonical exec path to desktop id; empty id marks an ambiguous binary
            QHash<QString, QString>               m_byExecBasename;
            QHash<QString, QString>               m_bySnap;    ///< Snap package name to desktop id
            QHash<Process::Identity, QString>     m_resolved;  ///< Own-match result per process, empty string when nothing matched
            QHash<QString, QIcon>                 m_icons;
            QFileSystemWatcher                   *m_watcher { nullptr };
            QTimer                               *m_rescanTimer { nullptr };

            void ensureIndexed();
            void buildIndex();
            QStringList applicationDirs() const;
            void indexDirectory(const QString &dir);
            bool parseDesktopFile(const QString &path, DesktopEntry &out) const;
            QString resolveExec(const QString &exec) const;
            QString resolveOwn(const Process &proc) const;
            static QString unescapeUnitName(const QString &name);
    };
}

#endif // OS_APPREGISTRY_H
