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

#ifndef PROCESSESWIDGET_H
#define PROCESSESWIDGET_H

#include "os/processmodel.h"
#include "os/processtreemodel.h"
#include "os/processfilterproxy.h"
#include "os/processrefreshservice.h"

#include <QModelIndex>
#include <QHeaderView>
#include <QSet>
#include <QSortFilterProxyModel>
#include <QStackedWidget>
#include <QTimer>
#include <QTreeView>
#include <QWidget>

#include <functional>

QT_BEGIN_NAMESPACE
namespace Ui
{
    class ProcessesWidget;
}
QT_END_NAMESPACE

//! Houses both table and tree widgets with lists of processes, the table and tree all reuse
//! same context menu and filters, but are two separate widgets that user can toggle between
//!
//! we are only updating the active widget
class ProcessesWidget : public QWidget
{
    Q_OBJECT

    public:
        explicit ProcessesWidget(OS::ProcessRefreshService *processRefreshService, QWidget *parent = nullptr);
        ~ProcessesWidget();
        void SetActive(bool active);
        bool IsActive() const { return this->m_active; }
        void ClearSearchFilter();
        bool SelectProcessByPid(pid_t pid);

    private slots:
        void onTimerTick();
        void onRefreshFinished(int consumer, quint64 token, const QList<OS::Process> &processes);
        void onHeaderContextMenu(const QPoint &pos);
        void onTableContextMenu(const QPoint &pos);
        void onTreeContextMenu(const QPoint &pos);
        void updateStatusBar();

    private:
        Ui::ProcessesWidget      *ui;
        OS::ProcessModel         *m_model;
        OS::ProcessTreeModel     *m_treeModel;
        OS::ProcessFilterProxy   *m_proxy;
        QSortFilterProxyModel    *m_treeProxy;
        QTimer                   *m_refreshTimer;
        OS::ProcessRefreshService *m_processRefreshService { nullptr };
        QStackedWidget           *m_viewStack { nullptr };
        QTreeView                *m_treeView { nullptr };
        bool                      m_active { false };
        bool                      m_tableContextMenuOpen { false };
        bool                      m_treeViewMode { false };
        bool                      m_refreshInFlight { false };
        bool                      m_refreshPending { false };
        bool                      m_tableHeaderPersistenceEnabled { false };
        bool                      m_treeHeaderPersistenceEnabled { false };
        quint64                   m_refreshToken { 0 };
        QList<OS::Process>        m_lastProcessSnapshot;
        QModelIndex               m_contextMenuTargetIndex;

        void setupTable();
        void startRefresh();
        void setTreeViewMode(bool enabled);
        void showHeaderContextMenu(QHeaderView *header, int columnCount, const std::function<QString(int)> &titleForColumn, const QPoint &pos);
        void saveTableHeaderState() const;
        void saveTreeHeaderState() const;
        void syncProcessColumnVisibility(int column, bool hidden);
        void syncAllProcessColumnVisibility();
        void updateIOMetricsEnabledState(bool triggerImmediateRefresh = true);
        bool anyIOMetricsColumnVisible() const;
        bool selectProcessInTree(pid_t pid);
        bool selectProcessInTable(pid_t pid);
        QVariant tableSelectionKeyFromProxy(const QModelIndex &proxyKeyIndex) const;
        void copyRowSelectionToClipboard();
        void copyCellSelectionToClipboard();
        void copySelectedPidsToClipboard();
        void terminateSelectedProcesses();
        void killSelectedProcesses();
        void promptAndSendCustomSignal();
        void runNewTask();
        void openTerminal();
        void setShowKernelTasks(bool checked);
        void setShowOtherUsersProcesses(bool checked);
        void captureExpandedTreePids(const QModelIndex &parentProxy, QSet<pid_t> &expandedPids) const;
        void restoreExpandedTreePids(const QModelIndex &sourceParent, const QSet<pid_t> &expandedPids);
        void restoreTreeStateDeferred(const QSet<pid_t> &expandedPids, const QList<pid_t> &treeSelection, pid_t treeCurrentPid, int treeScroll);

        /// Collect PIDs of all currently selected rows.
        QList<pid_t> selectedPids() const;
        QList<OS::Process> selectedProcesses() const;

        /// Send signal to all selected processes; show error dialog on failure.
        void sendSignalToSelected(int signal);

        /// Open renice dialog for all selected processes.
        void reniceSelected();
        bool startDetachedCommand(const QString &command, QString *error = nullptr) const;
        static QString findTerminalExecutable();
};

#endif // PROCESSESWIDGET_H
