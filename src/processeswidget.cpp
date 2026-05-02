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

#include "processeswidget.h"
#include "os/processhelper.h"
#include "configuration.h"
#include "globals.h"
#include "logger.h"
#include "runtaskdialog.h"
#include "ui_processeswidget.h"
#include "ui/uihelper.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QHeaderView>
#include <QInputDialog>
#include <QMenu>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QScrollBar>
#include <QSortFilterProxyModel>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QStyle>
#include <QTreeView>
#include <QVBoxLayout>
#include <QFileInfo>
#include <QMetaObject>

#include <algorithm>
#include <functional>
#include <signal.h>
#include <unistd.h>

ProcessesWidget::ProcessesWidget(OS::ProcessRefreshService *processRefreshService, QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ProcessesWidget)
    , m_model(new OS::ProcessModel(this))
    , m_treeModel(new OS::ProcessTreeModel(this))
    , m_proxy(new OS::ProcessFilterProxy(this))
    , m_treeProxy(new QSortFilterProxyModel(this))
    , m_refreshTimer(new QTimer(this))
    , m_processRefreshService(processRefreshService)
{
    this->ui->setupUi(this);

    this->setupTable();

    connect(this->ui->searchEdit, &QLineEdit::textChanged, this, [this](const QString &text)
    {
        this->m_proxy->setFilterFixedString(text);
        this->m_treeProxy->setFilterFixedString(text);
    });
    connect(this->ui->runNewTaskButton, &QPushButton::clicked, this, &ProcessesWidget::runNewTask);
    connect(this->ui->openTerminalButton, &QPushButton::clicked, this, &ProcessesWidget::openTerminal);
    connect(this->m_refreshTimer, &QTimer::timeout, this, &ProcessesWidget::onTimerTick);
    connect(this->m_processRefreshService, &OS::ProcessRefreshService::snapshotReady, this, &ProcessesWidget::onRefreshFinished);

    // Update status bar whenever the proxy's visible row count changes
    // (model reset after refresh, or filter toggle)
    connect(this->m_proxy, &QAbstractItemModel::modelReset, this, &ProcessesWidget::updateStatusBar);
    connect(this->m_proxy, &QAbstractItemModel::rowsInserted, this, &ProcessesWidget::updateStatusBar);
    connect(this->m_proxy, &QAbstractItemModel::rowsRemoved, this, &ProcessesWidget::updateStatusBar);
    connect(this->m_treeProxy, &QAbstractItemModel::modelReset, this, &ProcessesWidget::updateStatusBar);
    connect(this->m_treeProxy, &QAbstractItemModel::rowsInserted, this, &ProcessesWidget::updateStatusBar);
    connect(this->m_treeProxy, &QAbstractItemModel::rowsRemoved, this, &ProcessesWidget::updateStatusBar);

    // MainWindow controls active/inactive state based on current top tab.
}

ProcessesWidget::~ProcessesWidget()
{
    delete this->ui;
}

void ProcessesWidget::ClearSearchFilter()
{
    this->ui->searchEdit->clear();
}

bool ProcessesWidget::SelectProcessByPid(pid_t pid)
{
    return this->m_treeViewMode ? this->selectProcessInTree(pid) : this->selectProcessInTable(pid);
}

bool ProcessesWidget::selectProcessInTable(pid_t pid)
{
    if (pid <= 0)
        return false;

    for (int row = 0; row < this->m_model->rowCount(); ++row)
    {
        const QModelIndex sourceIdx = this->m_model->index(row, OS::ProcessModel::ColPid);
        if (!sourceIdx.isValid())
            continue;
        if (static_cast<pid_t>(this->m_model->data(sourceIdx, Qt::UserRole).toLongLong()) != pid)
            continue;

        const QModelIndex proxyIdx = this->m_proxy->mapFromSource(sourceIdx);
        if (!proxyIdx.isValid())
            return false;

        QItemSelectionModel *selectionModel = this->ui->tableView->selectionModel();
        if (!selectionModel)
            return false;

        selectionModel->clearSelection();
        selectionModel->select(proxyIdx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        selectionModel->setCurrentIndex(proxyIdx, QItemSelectionModel::NoUpdate);
        this->ui->tableView->scrollTo(proxyIdx, QAbstractItemView::PositionAtCenter);
        return true;
    }

    return false;
}

bool ProcessesWidget::selectProcessInTree(pid_t pid)
{
    if (pid <= 0)
        return false;

    const QModelIndex sourceIdx = this->m_treeModel->IndexForPid(pid);
    if (!sourceIdx.isValid())
        return false;

    const QModelIndex proxyIdx = this->m_treeProxy->mapFromSource(sourceIdx);
    if (!proxyIdx.isValid())
        return false;

    QItemSelectionModel *selectionModel = this->m_treeView->selectionModel();
    if (!selectionModel)
        return false;

    selectionModel->clearSelection();
    selectionModel->select(proxyIdx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    selectionModel->setCurrentIndex(proxyIdx, QItemSelectionModel::NoUpdate);
    this->m_treeView->scrollTo(proxyIdx, QAbstractItemView::PositionAtCenter);
    this->m_treeView->expand(proxyIdx.parent());
    return true;
}

// ── Private setup ─────────────────────────────────────────────────────────────

void ProcessesWidget::setupTable()
{
    this->m_proxy->setSourceModel(this->m_model);
    this->m_treeProxy->setSourceModel(this->m_treeModel);
    this->m_treeProxy->setSortRole(Qt::UserRole);
    this->m_treeProxy->setFilterRole(Qt::DisplayRole);
    this->m_treeProxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    this->m_treeProxy->setFilterKeyColumn(-1);
    this->m_treeProxy->setRecursiveFilteringEnabled(true);

    // Load persisted view toggles
    this->m_proxy->ShowKernelTasks     = CFG->ShowKernelTasks;
    this->m_proxy->ShowOtherUsersProcs = CFG->ShowOtherUsersProcs;
    this->m_model->SetShowKernelTasks(CFG->ShowKernelTasks);
    this->m_model->SetShowOtherUsersProcs(CFG->ShowOtherUsersProcs);

    QTableView *tv = this->ui->tableView;
    this->m_viewStack = new QStackedWidget(this);
    this->m_treeView = new QTreeView(this->m_viewStack);

    if (QVBoxLayout *vl = qobject_cast<QVBoxLayout *>(this->layout()))
    {
        const int tablePos = vl->indexOf(tv);
        vl->removeWidget(tv);
        this->m_viewStack->addWidget(tv);
        this->m_viewStack->addWidget(this->m_treeView);
        vl->insertWidget(tablePos, this->m_viewStack);
    }

    tv->setModel(this->m_proxy);
    tv->sortByColumn(CFG->ProcessListSortColumn, static_cast<Qt::SortOrder>(CFG->ProcessListSortOrder));

    QHeaderView *hv = tv->horizontalHeader();
    hv->setSectionsMovable(true);
    hv->setStretchLastSection(false);
    hv->setContextMenuPolicy(Qt::CustomContextMenu);
    hv->setSectionResizeMode(QHeaderView::Interactive);
    hv->setSectionResizeMode(OS::ProcessModel::ColCmdline, QHeaderView::Stretch);
    connect(hv, &QHeaderView::customContextMenuRequested, this, [this, hv](const QPoint &pos)
    {
        this->showHeaderContextMenu(hv, OS::ProcessModel::ColCount, [this](int col)
        {
            return this->m_model->headerData(col, Qt::Horizontal).toString();
        }, pos);
    });
    connect(hv, &QHeaderView::sectionMoved, this, [this]() { this->saveTableHeaderState(); });
    connect(hv, &QHeaderView::sectionResized, this, [this]() { this->saveTableHeaderState(); });

    connect(hv, &QHeaderView::sortIndicatorChanged, this, [](int column, Qt::SortOrder order)
    {
        CFG->ProcessListSortColumn = column;
        CFG->ProcessListSortOrder  = static_cast<int>(order);
    });

    tv->verticalHeader()->hide();
    tv->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tv->setSelectionMode(QAbstractItemView::ExtendedSelection);
    tv->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(tv, &QTableView::customContextMenuRequested, this, &ProcessesWidget::onTableContextMenu);

    // Reasonable default column widths
    tv->setColumnWidth(OS::ProcessModel::ColPid,      60);
    tv->setColumnWidth(OS::ProcessModel::ColName,    160);
    tv->setColumnWidth(OS::ProcessModel::ColUser,     90);
    tv->setColumnWidth(OS::ProcessModel::ColState,    90);
    tv->setColumnWidth(OS::ProcessModel::ColCpu,      65);
    tv->setColumnWidth(OS::ProcessModel::ColMemRss,   80);
    tv->setColumnWidth(OS::ProcessModel::ColMemVirt,  80);
    tv->setColumnWidth(OS::ProcessModel::ColIoReads,  90);
    tv->setColumnWidth(OS::ProcessModel::ColIoWrites, 90);
    tv->setColumnWidth(OS::ProcessModel::ColIoReadsPerSec, 100);
    tv->setColumnWidth(OS::ProcessModel::ColIoWritesPerSec, 100);
    tv->setColumnWidth(OS::ProcessModel::ColThreads,  65);
    tv->setColumnWidth(OS::ProcessModel::ColPriority, 65);
    tv->setColumnWidth(OS::ProcessModel::ColNice,     50);

    // Hide less-common columns by default
    hv->hideSection(OS::ProcessModel::ColMemVirt);
    hv->hideSection(OS::ProcessModel::ColPriority);
    hv->hideSection(OS::ProcessModel::ColNice);
    hv->hideSection(OS::ProcessModel::ColIoReads);
    hv->hideSection(OS::ProcessModel::ColIoWrites);
    hv->hideSection(OS::ProcessModel::ColIoReadsPerSec);
    hv->hideSection(OS::ProcessModel::ColIoWritesPerSec);
    if (!CFG->ProcessListHeaderState.isEmpty())
    {
        hv->restoreState(CFG->ProcessListHeaderState);
    }
    this->m_tableHeaderPersistenceEnabled = true;

    this->m_treeView->setModel(this->m_treeProxy);
    this->m_treeView->setSortingEnabled(true);
    this->m_treeView->sortByColumn(CFG->ProcessListSortColumn, static_cast<Qt::SortOrder>(CFG->ProcessListSortOrder));
    this->m_treeView->setAlternatingRowColors(true);
    this->m_treeView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    this->m_treeView->setSelectionBehavior(QAbstractItemView::SelectRows);
    this->m_treeView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    this->m_treeView->setContextMenuPolicy(Qt::CustomContextMenu);
    this->m_treeView->setRootIsDecorated(true);
    this->m_treeView->setItemsExpandable(true);
    this->m_treeView->setAnimated(false);
    this->m_treeView->setUniformRowHeights(true);
    QHeaderView *treeHeader = this->m_treeView->header();
    treeHeader->setSectionsMovable(true);
    treeHeader->setContextMenuPolicy(Qt::CustomContextMenu);
    treeHeader->setSectionResizeMode(QHeaderView::Interactive);
    treeHeader->setStretchLastSection(false);
    treeHeader->setSectionResizeMode(OS::ProcessTreeModel::ColCmdline, QHeaderView::Stretch);
    connect(treeHeader, &QHeaderView::customContextMenuRequested, this, [this, treeHeader](const QPoint &pos)
    {
        this->showHeaderContextMenu(treeHeader, OS::ProcessTreeModel::ColCount, [this](int col)
        {
            return this->m_treeModel->headerData(col, Qt::Horizontal).toString();
        }, pos);
    });
    connect(treeHeader, &QHeaderView::sectionMoved, this, [this]() { this->saveTreeHeaderState(); });
    connect(treeHeader, &QHeaderView::sectionResized, this, [this]() { this->saveTreeHeaderState(); });
    this->m_treeView->setColumnWidth(OS::ProcessTreeModel::ColPid, 60);
    this->m_treeView->setColumnWidth(OS::ProcessTreeModel::ColName, 160);
    this->m_treeView->setColumnWidth(OS::ProcessTreeModel::ColUser, 90);
    this->m_treeView->setColumnWidth(OS::ProcessTreeModel::ColState, 90);
    this->m_treeView->setColumnWidth(OS::ProcessTreeModel::ColCpu, 65);
    this->m_treeView->setColumnWidth(OS::ProcessTreeModel::ColMemRss, 80);
    this->m_treeView->setColumnWidth(OS::ProcessTreeModel::ColMemVirt, 80);
    this->m_treeView->setColumnWidth(OS::ProcessTreeModel::ColIoReads, 90);
    this->m_treeView->setColumnWidth(OS::ProcessTreeModel::ColIoWrites, 90);
    this->m_treeView->setColumnWidth(OS::ProcessTreeModel::ColIoReadsPerSec, 100);
    this->m_treeView->setColumnWidth(OS::ProcessTreeModel::ColIoWritesPerSec, 100);
    this->m_treeView->setColumnWidth(OS::ProcessTreeModel::ColThreads, 65);
    this->m_treeView->setColumnWidth(OS::ProcessTreeModel::ColPriority, 65);
    this->m_treeView->setColumnWidth(OS::ProcessTreeModel::ColNice, 50);
    this->m_treeView->setColumnHidden(OS::ProcessTreeModel::ColMemVirt, true);
    this->m_treeView->setColumnHidden(OS::ProcessTreeModel::ColPriority, true);
    this->m_treeView->setColumnHidden(OS::ProcessTreeModel::ColNice, true);
    this->m_treeView->setColumnHidden(OS::ProcessTreeModel::ColIoReads, true);
    this->m_treeView->setColumnHidden(OS::ProcessTreeModel::ColIoWrites, true);
    this->m_treeView->setColumnHidden(OS::ProcessTreeModel::ColIoReadsPerSec, true);
    this->m_treeView->setColumnHidden(OS::ProcessTreeModel::ColIoWritesPerSec, true);
    if (!CFG->ProcessTreeHeaderState.isEmpty())
    {
        treeHeader->restoreState(CFG->ProcessTreeHeaderState);
    }
    this->syncAllProcessColumnVisibility();
    this->m_treeHeaderPersistenceEnabled = true;
    connect(this->m_treeView, &QTreeView::customContextMenuRequested, this, &ProcessesWidget::onTreeContextMenu);
    this->updateIOMetricsEnabledState(false);

    this->setTreeViewMode(CFG->ProcessTreeView);
}

void ProcessesWidget::setTreeViewMode(bool enabled)
{
    this->m_treeViewMode = enabled;
    CFG->ProcessTreeView = enabled;

    if (enabled)
        this->m_treeModel->SetProcesses(this->m_lastProcessSnapshot.isEmpty() ? this->m_model->GetProcesses() : this->m_lastProcessSnapshot);

    if (!this->m_viewStack)
        return;
    this->m_viewStack->setCurrentWidget(enabled ? static_cast<QWidget *>(this->m_treeView)
                                                : static_cast<QWidget *>(this->ui->tableView));
}

void ProcessesWidget::SetActive(bool active)
{
    if (this->m_active == active)
        return;

    this->m_active = active;
    if (active)
    {
        // Refresh immediately when user switches to Processes tab.
        this->startRefresh();
        this->m_refreshTimer->start(CFG->RefreshRateMs);
    } else
    {
        this->m_refreshTimer->stop();
        this->m_refreshPending = false;
    }
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void ProcessesWidget::onTimerTick()
{
    if (CFG->RefreshPaused)
        return;
    this->startRefresh();
}

void ProcessesWidget::startRefresh()
{
    if (CFG->RefreshPaused)
    {
        this->m_refreshPending = false;
        return;
    }

    if (this->m_tableContextMenuOpen)
    {
        this->m_refreshPending = true;
        return;
    }

    if (this->m_refreshInFlight)
    {
        this->m_refreshPending = true;
        return;
    }

    this->m_refreshInFlight = true;
    this->m_refreshPending = false;
    ++this->m_refreshToken;

    OS::Process::LoadOptions options;
    options.IncludeKernelTasks = this->m_proxy->ShowKernelTasks;
    options.IncludeOtherUsers = this->m_proxy->ShowOtherUsersProcs;
    options.MyUID = ::getuid();
    options.CollectIOMetrics = CFG->IOMetricsEnabled;
    options.IsSuperuser = CFG->IsSuperuser;
    options.EffectiveUID = CFG->EUID;

    this->m_processRefreshService->RequestSnapshot(OS::ProcessRefreshService::Consumer::Processes,
                                                   this->m_refreshToken,
                                                   options);
}

void ProcessesWidget::onRefreshFinished(int consumer, quint64 token, const QList<OS::Process> &processes)
{
    if (consumer != static_cast<int>(OS::ProcessRefreshService::Consumer::Processes) || token != this->m_refreshToken)
        return;

    this->m_refreshInFlight = false;

    if (!this->m_active)
        return;

    if (this->m_tableContextMenuOpen)
    {
        this->m_refreshPending = true;
        return;
    }

    UIHelper::TableSelectionSnapshot tableSnapshot;
    QList<pid_t> treeSelection;
    QSet<pid_t> expandedPids;
    pid_t treeCurrentPid = 0;
    int treeScroll = 0;

    if (!this->m_treeViewMode)
    {
        tableSnapshot = UIHelper::CaptureTableSelection(this->ui->tableView, OS::ProcessModel::ColPid, std::bind(&ProcessesWidget::tableSelectionKeyFromProxy, this, std::placeholders::_1));
    } else
    {
        treeSelection = this->selectedPids();
        this->captureExpandedTreePids(QModelIndex(), expandedPids);

        if (QItemSelectionModel *sel = this->m_treeView->selectionModel())
        {
            const QModelIndex cur = sel->currentIndex();
            if (cur.isValid())
            {
                const QModelIndex src = this->m_treeProxy->mapToSource(cur.sibling(cur.row(), OS::ProcessTreeModel::ColPid));
                if (src.isValid())
                    treeCurrentPid = static_cast<pid_t>(this->m_treeModel->data(src, Qt::UserRole).toLongLong());
            }
        }
        if (QScrollBar *sb = this->m_treeView->verticalScrollBar())
            treeScroll = sb->value();
    }
    this->m_lastProcessSnapshot = processes;
    this->m_model->SetProcesses(this->m_lastProcessSnapshot);

    if (this->m_treeViewMode)
    {
        this->m_treeModel->SetProcesses(this->m_lastProcessSnapshot);
        this->restoreTreeStateDeferred(expandedPids, treeSelection, treeCurrentPid, treeScroll);
    } else
    {
        UIHelper::RestoreTableSelection(
            this->ui->tableView,
            OS::ProcessModel::ColPid,
            this->m_model->rowCount(),
            [this](int row) -> QModelIndex { return this->m_model->index(row, OS::ProcessModel::ColPid); },
            [this](const QModelIndex &sourceIndex) -> QModelIndex { return this->m_proxy->mapFromSource(sourceIndex); },
            [this](const QModelIndex &sourceKeyIndex) -> QVariant { return this->m_model->data(sourceKeyIndex, Qt::UserRole); },
            tableSnapshot);
    }

    if (this->m_active && this->m_refreshPending && !CFG->RefreshPaused)
    {
        this->m_refreshPending = false;
        QMetaObject::invokeMethod(this, &ProcessesWidget::startRefresh, Qt::QueuedConnection);
    }
}

void ProcessesWidget::onHeaderContextMenu(const QPoint &pos)
{
    QHeaderView *hv = this->ui->tableView->horizontalHeader();
    this->showHeaderContextMenu(hv, OS::ProcessModel::ColCount, [this](int col)
    {
        return this->m_model->headerData(col, Qt::Horizontal).toString();
    }, pos);
}

void ProcessesWidget::showHeaderContextMenu(QHeaderView *header, int columnCount, const std::function<QString(int)> &titleForColumn, const QPoint &pos)
{
    if (!header)
        return;

    QMenu menu(this);

    for (int col = 0; col < columnCount; ++col)
    {
        QAction *action = menu.addAction(titleForColumn(col));
        action->setCheckable(true);
        action->setChecked(!header->isSectionHidden(col));
        action->setData(col);
    }

    UIHelper::AddGlobalContextMenuItems(&menu, this);

    const QAction *chosen = menu.exec(header->mapToGlobal(pos));
    if (!chosen)
        return;

    const int col = chosen->data().toInt();
    this->syncProcessColumnVisibility(col, !chosen->isChecked());
    this->saveTableHeaderState();
    this->saveTreeHeaderState();
    this->updateIOMetricsEnabledState();
}

void ProcessesWidget::saveTableHeaderState() const
{
    if (!this->m_tableHeaderPersistenceEnabled)
        return;

    if (QHeaderView *header = this->ui->tableView ? this->ui->tableView->horizontalHeader() : nullptr)
        CFG->ProcessListHeaderState = header->saveState();
}

void ProcessesWidget::saveTreeHeaderState() const
{
    if (!this->m_treeHeaderPersistenceEnabled)
        return;

    if (this->m_treeView && this->m_treeView->header())
        CFG->ProcessTreeHeaderState = this->m_treeView->header()->saveState();
}

void ProcessesWidget::syncProcessColumnVisibility(int column, bool hidden)
{
    if (column < 0 || column >= OS::ProcessModel::ColCount)
        return;

    if (QHeaderView *tableHeader = this->ui && this->ui->tableView ? this->ui->tableView->horizontalHeader() : nullptr)
        tableHeader->setSectionHidden(column, hidden);
    if (this->m_treeView)
        this->m_treeView->setColumnHidden(column, hidden);
}

void ProcessesWidget::syncAllProcessColumnVisibility()
{
    if (!this->ui || !this->ui->tableView || !this->m_treeView)
        return;

    QHeaderView *tableHeader = this->ui->tableView->horizontalHeader();
    QHeaderView *treeHeader = this->m_treeView->header();
    if (!tableHeader || !treeHeader)
        return;

    for (int col = 0; col < OS::ProcessModel::ColCount; ++col)
    {
        const bool visible = !tableHeader->isSectionHidden(col) || !treeHeader->isSectionHidden(col);
        this->syncProcessColumnVisibility(col, !visible);
    }
}

bool ProcessesWidget::anyIOMetricsColumnVisible() const
{
    const QHeaderView *tableHeader = this->ui && this->ui->tableView ? this->ui->tableView->horizontalHeader() : nullptr;
    if (!tableHeader)
        return false;

    return !tableHeader->isSectionHidden(OS::ProcessModel::ColIoReads)
           || !tableHeader->isSectionHidden(OS::ProcessModel::ColIoWrites)
           || !tableHeader->isSectionHidden(OS::ProcessModel::ColIoReadsPerSec)
           || !tableHeader->isSectionHidden(OS::ProcessModel::ColIoWritesPerSec);
}

void ProcessesWidget::updateIOMetricsEnabledState(bool triggerImmediateRefresh)
{
    const bool enabled = this->anyIOMetricsColumnVisible();
    const bool changed = (CFG->IOMetricsEnabled != enabled);

    CFG->IOMetricsEnabled = enabled;

    if (changed && enabled && triggerImmediateRefresh && this->m_active && !CFG->RefreshPaused && !this->m_tableContextMenuOpen)
        this->startRefresh();
}

void ProcessesWidget::onTableContextMenu(const QPoint &pos)
{
    // Pause refresh during context menu so we don't loose the selection
    this->m_tableContextMenuOpen = true;
    QHash<QAction *, int> refreshIntervalActions;
    QAction *pausedRefreshAction = nullptr;

    const QModelIndex clickedIndex = this->ui->tableView->indexAt(pos);
    const QModelIndex targetIndex = clickedIndex.isValid() ? clickedIndex : this->ui->tableView->currentIndex();
    this->m_contextMenuTargetIndex = targetIndex;
    const bool hasTargetCell = targetIndex.isValid();
    const QModelIndexList selectedRowsIdx = this->ui->tableView->selectionModel()->selectedRows();
    QList<int> selectedRows;
    selectedRows.reserve(selectedRowsIdx.size());
    for (const QModelIndex &idx : selectedRowsIdx)
        selectedRows.append(idx.row());
    std::sort(selectedRows.begin(), selectedRows.end());
    const bool hasSelectedRows = !selectedRows.isEmpty();
    const bool multipleRowsSelected = selectedRows.size() > 1;
    const bool hasRowTarget = hasTargetCell || hasSelectedRows;

    const QList<pid_t> pids = this->selectedPids();
    const bool hasSelection = !pids.isEmpty();

    QMenu menu(this);

    // ── Copy submenu ────────────────────────────────────────────────────────
    QMenu *copyMenu = menu.addMenu(tr("Copy"));

    QAction *copyRowAct = copyMenu->addAction(tr("Entire row"));
    copyRowAct->setEnabled(hasRowTarget);
    connect(copyRowAct, &QAction::triggered, this, &ProcessesWidget::copyRowSelectionToClipboard);

    QAction *copyCellAct = copyMenu->addAction(tr("Selected cell"));
    copyCellAct->setEnabled(hasTargetCell && !multipleRowsSelected);
    connect(copyCellAct, &QAction::triggered, this, &ProcessesWidget::copyCellSelectionToClipboard);

    QString copyPIDLabel;
    if (multipleRowsSelected)
        copyPIDLabel = tr("PIDs");
    else
        copyPIDLabel = tr("PID");

    QAction *copyPidAct = copyMenu->addAction(copyPIDLabel);
    copyPidAct->setEnabled(hasSelection);
    connect(copyPidAct, &QAction::triggered, this, &ProcessesWidget::copySelectedPidsToClipboard);

    menu.addSeparator();

    // ── View submenu — always visible ────────────────────────────────────────
    QMenu *viewMenu = menu.addMenu(tr("View"));

    QAction *kernelAct = viewMenu->addAction(tr("Kernel tasks"));
    kernelAct->setCheckable(true);
    kernelAct->setChecked(this->m_proxy->ShowKernelTasks);
    connect(kernelAct, &QAction::toggled, this, &ProcessesWidget::setShowKernelTasks);

    QAction *otherUsersAct = viewMenu->addAction(tr("Processes of other users"));
    otherUsersAct->setCheckable(true);
    otherUsersAct->setChecked(this->m_proxy->ShowOtherUsersProcs);
    connect(otherUsersAct, &QAction::toggled, this, &ProcessesWidget::setShowOtherUsersProcesses);

    viewMenu->addSeparator();
    QAction *tableModeAct = viewMenu->addAction(tr("Table view"));
    tableModeAct->setCheckable(true);
    tableModeAct->setChecked(!this->m_treeViewMode);
    connect(tableModeAct, &QAction::triggered, this, [this]() { this->setTreeViewMode(false); });

    QAction *treeModeAct = viewMenu->addAction(tr("Tree view"));
    treeModeAct->setCheckable(true);
    treeModeAct->setChecked(this->m_treeViewMode);
    connect(treeModeAct, &QAction::triggered, this, [this]() { this->setTreeViewMode(true); });
    // ── Send signal submenu — requires selection ────────────────────────────
    menu.addSeparator();
    QMenu *signalMenu = menu.addMenu(tr("Send signal"));
    signalMenu->setEnabled(hasSelection);

    struct { const char *label; int sig; } commonSignals[] =
    {
        { "SIGTERM  (15) — Terminate",   SIGTERM  },
        { "SIGKILL   (9) — Kill (force)", SIGKILL  },
        { "SIGHUP    (1) — Hangup",       SIGHUP   },
        { "SIGSTOP  (19) — Stop",         SIGSTOP  },
        { "SIGCONT  (18) — Continue",     SIGCONT  },
        { "SIGINT    (2) — Interrupt",    SIGINT   },
        { "SIGUSR1  (10) — User 1",       SIGUSR1  },
        { "SIGUSR2  (12) — User 2",       SIGUSR2  },
    };
    for (const auto &s : commonSignals)
    {
        QAction *a = signalMenu->addAction(tr(s.label));
        a->setData(s.sig);
        connect(a, &QAction::triggered, this, [this, s]()
        {
            this->sendSignalToSelected(s.sig);
        });
    }

    signalMenu->addSeparator();
    QAction *customSig = signalMenu->addAction(tr("Custom signal..."));
    connect(customSig, &QAction::triggered, this, &ProcessesWidget::promptAndSendCustomSignal);

    // ── Quick-access kill / term — requires selection ───────────────────────
    menu.addSeparator();
    QAction *termAction = menu.addAction(tr("Terminate  (SIGTERM)"));
    termAction->setEnabled(hasSelection);
    connect(termAction, &QAction::triggered, this, &ProcessesWidget::terminateSelectedProcesses);

    QAction *killAction = menu.addAction(tr("Kill  (SIGKILL)"));
    killAction->setEnabled(hasSelection);
    killAction->setIcon(style()->standardIcon(QStyle::SP_MessageBoxCritical));
    connect(killAction, &QAction::triggered, this, &ProcessesWidget::killSelectedProcesses);

    // ── Priority ─────────────────────────────────────────────────────────────
    menu.addSeparator();
    QAction *reniceAction = menu.addAction(tr("Change priority (renice)..."));
    reniceAction->setEnabled(hasSelection);
    connect(reniceAction, &QAction::triggered, this, &ProcessesWidget::reniceSelected);

    menu.addSeparator();
    QMenu *refreshMenu = menu.addMenu(tr("Refresh interval"));
    UIHelper::PopulateRefreshIntervalMenu(refreshMenu, refreshIntervalActions, pausedRefreshAction);
    UIHelper::AddGlobalContextMenuItems(&menu, this);

    QAction *picked = menu.exec(this->ui->tableView->viewport()->mapToGlobal(pos));
    UIHelper::ApplyRefreshIntervalAction(picked, refreshIntervalActions, pausedRefreshAction, this->m_refreshTimer, this->m_active);

    this->m_contextMenuTargetIndex = QModelIndex();
    this->m_tableContextMenuOpen = false;
    if (this->m_active && this->m_refreshPending && !CFG->RefreshPaused)
        this->startRefresh();
}

void ProcessesWidget::onTreeContextMenu(const QPoint &pos)
{
    this->m_tableContextMenuOpen = true;
    QMenu menu(this);
    QHash<QAction *, int> refreshIntervalActions;
    QAction *pausedRefreshAction = nullptr;

    QMenu *viewMenu = menu.addMenu(tr("View"));
    QAction *kernelAct = viewMenu->addAction(tr("Kernel tasks"));
    kernelAct->setCheckable(true);
    kernelAct->setChecked(this->m_proxy->ShowKernelTasks);
    connect(kernelAct, &QAction::toggled, this, &ProcessesWidget::setShowKernelTasks);

    QAction *otherUsersAct = viewMenu->addAction(tr("Processes of other users"));
    otherUsersAct->setCheckable(true);
    otherUsersAct->setChecked(this->m_proxy->ShowOtherUsersProcs);
    connect(otherUsersAct, &QAction::toggled, this, &ProcessesWidget::setShowOtherUsersProcesses);

    viewMenu->addSeparator();
    QAction *tableModeAct = viewMenu->addAction(tr("Table view"));
    tableModeAct->setCheckable(true);
    tableModeAct->setChecked(!this->m_treeViewMode);
    connect(tableModeAct, &QAction::triggered, this, [this]() { this->setTreeViewMode(false); });

    QAction *treeModeAct = viewMenu->addAction(tr("Tree view"));
    treeModeAct->setCheckable(true);
    treeModeAct->setChecked(this->m_treeViewMode);
    connect(treeModeAct, &QAction::triggered, this, [this]() { this->setTreeViewMode(true); });

    const QList<pid_t> pids = this->selectedPids();
    const bool hasSelection = !pids.isEmpty();
    menu.addSeparator();
    QAction *termAction = menu.addAction(tr("Terminate  (SIGTERM)"));
    termAction->setEnabled(hasSelection);
    connect(termAction, &QAction::triggered, this, &ProcessesWidget::terminateSelectedProcesses);

    QAction *killAction = menu.addAction(tr("Kill  (SIGKILL)"));
    killAction->setEnabled(hasSelection);
    killAction->setIcon(style()->standardIcon(QStyle::SP_MessageBoxCritical));
    connect(killAction, &QAction::triggered, this, &ProcessesWidget::killSelectedProcesses);

    QAction *reniceAction = menu.addAction(tr("Change priority (renice)..."));
    reniceAction->setEnabled(hasSelection);
    connect(reniceAction, &QAction::triggered, this, &ProcessesWidget::reniceSelected);

    menu.addSeparator();
    QMenu *refreshMenu = menu.addMenu(tr("Refresh interval"));
    UIHelper::PopulateRefreshIntervalMenu(refreshMenu, refreshIntervalActions, pausedRefreshAction);
    UIHelper::AddGlobalContextMenuItems(&menu, this);

    QAction *picked = menu.exec(this->m_treeView->viewport()->mapToGlobal(pos));
    UIHelper::ApplyRefreshIntervalAction(picked, refreshIntervalActions, pausedRefreshAction, this->m_refreshTimer, this->m_active);

    this->m_tableContextMenuOpen = false;
    if (this->m_active && this->m_refreshPending && !CFG->RefreshPaused)
        this->startRefresh();
}

// ── Helpers ───────────────────────────────────────────────────────────────────

void ProcessesWidget::updateStatusBar()
{
    const QList<OS::Process> &all = this->m_lastProcessSnapshot.isEmpty()
                                    ? this->m_model->GetProcesses()
                                    : this->m_lastProcessSnapshot;
    const int totalTasks = all.size();

    qlonglong totalThreads = 0;
    for (const OS::Process &p : all)
        totalThreads += qMax(0, p.Threads);
    const QString text = tr("Tasks: %1  Threads: %2").arg(totalTasks).arg(totalThreads);

    this->ui->statusLabel->setText(text);
}

void ProcessesWidget::copyRowSelectionToClipboard()
{
    if (!this->ui->tableView || !this->ui->tableView->model())
        return;

    QModelIndexList selectedRowsIdx = this->ui->tableView->selectionModel()
                                      ? this->ui->tableView->selectionModel()->selectedRows()
                                      : QModelIndexList();
    QList<int> selectedRows;
    selectedRows.reserve(selectedRowsIdx.size());
    for (const QModelIndex &idx : selectedRowsIdx)
        selectedRows.append(idx.row());
    std::sort(selectedRows.begin(), selectedRows.end());

    const QModelIndex targetIndex = this->m_contextMenuTargetIndex.isValid()
                                    ? this->m_contextMenuTargetIndex
                                    : this->ui->tableView->currentIndex();

    if (selectedRows.size() > 1)
    {
        QGuiApplication::clipboard()->setText(UIHelper::GetVisibleRowsText(this->ui->tableView, selectedRows));
        return;
    }

    const int row = targetIndex.isValid() ? targetIndex.row() : selectedRows.value(0, -1);
    QGuiApplication::clipboard()->setText(UIHelper::GetVisibleRowText(this->ui->tableView, row));
}

QVariant ProcessesWidget::tableSelectionKeyFromProxy(const QModelIndex &proxyKeyIndex) const
{
    const QModelIndex srcIdx = this->m_proxy->mapToSource(proxyKeyIndex);
    if (!srcIdx.isValid())
        return {};
    return this->m_model->data(srcIdx, Qt::UserRole);
}

void ProcessesWidget::copyCellSelectionToClipboard()
{
    if (!this->ui->tableView || !this->ui->tableView->model())
        return;

    const QModelIndex targetIndex = this->m_contextMenuTargetIndex.isValid()
                                    ? this->m_contextMenuTargetIndex
                                    : this->ui->tableView->currentIndex();
    if (!targetIndex.isValid())
        return;

    QGuiApplication::clipboard()->setText(this->ui->tableView->model()->data(targetIndex, Qt::DisplayRole).toString());
}

void ProcessesWidget::copySelectedPidsToClipboard()
{
    const QList<pid_t> pids = this->selectedPids();
    if (pids.isEmpty())
        return;

    QStringList pidStrings;
    pidStrings.reserve(pids.size());
    for (pid_t pid : pids)
        pidStrings << QString::number(pid);

    QGuiApplication::clipboard()->setText(pidStrings.join(' '));
}

void ProcessesWidget::terminateSelectedProcesses()
{
    this->sendSignalToSelected(SIGTERM);
}

void ProcessesWidget::killSelectedProcesses()
{
    this->sendSignalToSelected(SIGKILL);
}

void ProcessesWidget::promptAndSendCustomSignal()
{
    bool ok = false;
    const int sig = QInputDialog::getInt(
        this,
        tr("Send custom signal"),
        tr("Signal number:"),
        1, 1, 64, 1, &ok);
    if (ok)
        this->sendSignalToSelected(sig);
}

void ProcessesWidget::runNewTask()
{
    RunTaskDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    const QString command = dialog.Command();
    if (command.isEmpty())
        return;

    QString error;
    if (!this->startDetachedCommand(command, &error))
    {
        QMessageBox::warning(this, tr("Run new task failed"), tr("Failed to start command.\n\n%1").arg(error));
        return;
    }

    CFG->TaskHistory.removeAll(command);
    CFG->TaskHistory.prepend(command);
    while (CFG->TaskHistory.size() > TUX_MANAGER_TASK_HISTORY)
        CFG->TaskHistory.removeLast();
    CFG->Save();

    LOG_INFO(QString("Started detached task: %1").arg(command));
}

void ProcessesWidget::openTerminal()
{
    const QString terminal = findTerminalExecutable();
    if (terminal.isEmpty())
    {
        QMessageBox::warning(this, tr("Open terminal failed"), tr("No supported terminal emulator was found on this system."));
        return;
    }

    if (!QProcess::startDetached(terminal, {}))
    {
        QMessageBox::warning(this, tr("Open terminal failed"), tr("Failed to start terminal: %1").arg(terminal));
        return;
    }

    LOG_INFO(QString("Opened terminal via detached process: %1").arg(terminal));
}

void ProcessesWidget::setShowKernelTasks(bool checked)
{
    this->m_proxy->ShowKernelTasks = checked;
    this->m_model->SetShowKernelTasks(checked);
    CFG->ShowKernelTasks = checked;
    this->m_proxy->ApplyFilters();
    this->onTimerTick();
    LOG_DEBUG(QString("ShowKernelTasks = %1").arg(checked));
}

void ProcessesWidget::setShowOtherUsersProcesses(bool checked)
{
    this->m_proxy->ShowOtherUsersProcs = checked;
    this->m_model->SetShowOtherUsersProcs(checked);
    CFG->ShowOtherUsersProcs = checked;
    this->m_proxy->ApplyFilters();
    this->onTimerTick();
    LOG_DEBUG(QString("ShowOtherUsersProcs = %1").arg(checked));
}

void ProcessesWidget::captureExpandedTreePids(const QModelIndex &parentProxy, QSet<pid_t> &expandedPids) const
{
    const int rows = this->m_treeProxy->rowCount(parentProxy);
    for (int r = 0; r < rows; ++r)
    {
        const QModelIndex proxyIdx = this->m_treeProxy->index(r, OS::ProcessTreeModel::ColPid, parentProxy);
        if (!proxyIdx.isValid() || !this->m_treeView->isExpanded(proxyIdx))
            continue;

        const QModelIndex srcIdx = this->m_treeProxy->mapToSource(proxyIdx);
        if (srcIdx.isValid())
            expandedPids.insert(static_cast<pid_t>(this->m_treeModel->data(srcIdx, Qt::UserRole).toLongLong()));

        this->captureExpandedTreePids(proxyIdx, expandedPids);
    }
}

void ProcessesWidget::restoreExpandedTreePids(const QModelIndex &sourceParent, const QSet<pid_t> &expandedPids)
{
    const int rows = this->m_treeModel->rowCount(sourceParent);
    for (int r = 0; r < rows; ++r)
    {
        const QModelIndex srcIdx = this->m_treeModel->index(r, OS::ProcessTreeModel::ColPid, sourceParent);
        if (!srcIdx.isValid())
            continue;

        const pid_t pid = static_cast<pid_t>(this->m_treeModel->data(srcIdx, Qt::UserRole).toLongLong());
        const QModelIndex proxyIdx = this->m_treeProxy->mapFromSource(srcIdx);
        if (proxyIdx.isValid() && expandedPids.contains(pid))
            this->m_treeView->expand(proxyIdx);

        this->restoreExpandedTreePids(srcIdx, expandedPids);
    }
}

void ProcessesWidget::restoreTreeStateDeferred(const QSet<pid_t> &expandedPids,
                                               const QList<pid_t> &treeSelection,
                                               pid_t treeCurrentPid,
                                               int treeScroll)
{
    // Defer restoration until the proxy/view settle after model reset/sort.
    QTimer::singleShot(0, this, [this, expandedPids, treeSelection, treeCurrentPid, treeScroll]()
    {
        this->restoreExpandedTreePids(QModelIndex(), expandedPids);

        if (QItemSelectionModel *sel = this->m_treeView->selectionModel())
        {
            sel->clearSelection();
            for (pid_t pid : treeSelection)
            {
                const QModelIndex src = this->m_treeModel->IndexForPid(pid);
                const QModelIndex proxy = this->m_treeProxy->mapFromSource(src);
                if (proxy.isValid())
                    sel->select(proxy, QItemSelectionModel::Select | QItemSelectionModel::Rows);
            }

            if (treeCurrentPid > 0)
            {
                const QModelIndex src = this->m_treeModel->IndexForPid(treeCurrentPid);
                const QModelIndex proxy = this->m_treeProxy->mapFromSource(src);
                if (proxy.isValid())
                    sel->setCurrentIndex(proxy, QItemSelectionModel::NoUpdate);
            }
        }

        if (QScrollBar *sb = this->m_treeView->verticalScrollBar())
            sb->setValue(treeScroll);
    });
}

QList<pid_t> ProcessesWidget::selectedPids() const
{
    QList<pid_t> pids;
    if (!this->m_treeViewMode)
    {
        const QModelIndexList rows = this->ui->tableView->selectionModel()->selectedRows(OS::ProcessModel::ColPid);
        pids.reserve(rows.size());
        for (const QModelIndex &proxyIdx : rows)
        {
            const QModelIndex srcIdx = this->m_proxy->mapToSource(proxyIdx);
            const QVariant v = this->m_model->data(srcIdx, Qt::UserRole);
            pids.append(static_cast<pid_t>(v.toLongLong()));
        }
    } else if (this->m_treeView && this->m_treeView->selectionModel())
    {
        const QModelIndexList rows = this->m_treeView->selectionModel()->selectedRows(OS::ProcessTreeModel::ColPid);
        pids.reserve(rows.size());
        for (const QModelIndex &proxyIdx : rows)
        {
            const QModelIndex srcIdx = this->m_treeProxy->mapToSource(proxyIdx);
            const QVariant v = this->m_treeModel->data(srcIdx, Qt::UserRole);
            pids.append(static_cast<pid_t>(v.toLongLong()));
        }
    }
    return pids;
}

QList<OS::Process> ProcessesWidget::selectedProcesses() const
{
    const QList<pid_t> pids = this->selectedPids();
    if (pids.isEmpty())
        return {};

    const QList<OS::Process> &all = this->m_lastProcessSnapshot.isEmpty()
                                    ? this->m_model->GetProcesses()
                                    : this->m_lastProcessSnapshot;
    QHash<pid_t, const OS::Process *> byPid;
    byPid.reserve(all.size());
    for (const OS::Process &proc : all)
        byPid.insert(proc.PID, &proc);

    QList<OS::Process> selected;
    selected.reserve(pids.size());
    for (pid_t pid : pids)
    {
        const auto it = byPid.constFind(pid);
        if (it != byPid.cend() && it.value())
            selected.append(*it.value());
    }
    return selected;
}

void ProcessesWidget::sendSignalToSelected(int signal)
{
    const QList<pid_t> pids = this->selectedPids();
    if (pids.isEmpty())
        return;
    const QList<OS::Process> selected = this->selectedProcesses();

    QStringList pidStrings;
    pidStrings.reserve(pids.size());
    for (pid_t pid : pids)
        pidStrings << QString::number(pid);

    const QString signalLabel = OS::ProcessHelper::GetSignalName(signal);
    const QString signalText = signalLabel.isEmpty() ? QString::number(signal) : signalLabel;

    QString body;
    if (pids.size() == 1)
    {
        body = tr("You are about to send signal %1 to process PID %2.\n\n"
                  "Do you want to continue?")
                   .arg(signalText, pidStrings.first());
    } else
    {
        body = tr("You are about to send signal %1 to %2 selected processes.\n"
                  "This will affect all selected processes.\n\n"
                  "PIDs: %3\n\n"
                  "Do you want to continue?")
                   .arg(signalText, QString::number(pids.size()), pidStrings.join(", "));
    }

    if (!CFG->IsSuperuser)
    {
        bool targetsOtherUser = false;
        QStringList otherUsers;
        for (const OS::Process &proc : selected)
        {
            if (proc.UID == CFG->EUID)
                continue;
            targetsOtherUser = true;
            if (!proc.User.isEmpty() && !otherUsers.contains(proc.User))
                otherUsers.append(proc.User);
        }

        if (targetsOtherUser)
        {
            const QString userHint = otherUsers.isEmpty()
                                     ? QString()
                                     : tr("\nTarget owners: %1").arg(otherUsers.join(", "));
            body += tr("\n\nWarning: one or more selected processes are owned by another user. "
                       "This application is running without superuser privileges, so this signal will most likely be rejected and nothing will happen.")
                    + userHint;
        }
    }

    const QMessageBox::StandardButton answer = QMessageBox::warning(this, tr("Confirm Signal"), body, QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes)
        return;

    QStringList errors;
    for (pid_t pid : pids)
    {
        QString err;
        if (!OS::ProcessHelper::SendSignal(pid, signal, err))
        {
            LOG_WARN(err);
            errors << err;
        } else
        {
            LOG_INFO(QString("Sent %1 to PID %2").arg(OS::ProcessHelper::GetSignalName(signal)).arg(pid));
        }
    }

    if (!errors.isEmpty())
    {
        QMessageBox::warning(this, tr("Signal failed"), errors.join('\n'));
    }
}

void ProcessesWidget::reniceSelected()
{
    const QList<pid_t> pids = this->selectedPids();
    if (pids.isEmpty())
        return;

    bool ok;
    const int nice = QInputDialog::getInt(this, tr("Change priority"), tr("Nice value (-20 = highest priority, 19 = lowest):"), 0, -20, 19, 1, &ok);
    if (!ok)
        return;

    QStringList errors;
    for (pid_t pid : pids)
    {
        QString err;
        if (!OS::ProcessHelper::Renice(pid, nice, err))
        {
            LOG_WARN(err);
            errors << err;
        } else
        {
            LOG_INFO(QString("Reniced PID %1 to nice=%2").arg(pid).arg(nice));
        }
    }

    if (!errors.isEmpty())
    {
        QMessageBox::warning(this, tr("Renice failed"), errors.join('\n'));
    }
}

bool ProcessesWidget::startDetachedCommand(const QString &command, QString *error) const
{
    if (error)
        error->clear();

    if (command.trimmed().isEmpty())
    {
        if (error)
            *error = tr("Command is empty");
        return false;
    }

    if (QProcess::startDetached("/bin/sh", { "-c", command }))
        return true;

    if (error)
        *error = tr("Unable to start /bin/sh -c %1").arg(command);
    return false;
}

QString ProcessesWidget::findTerminalExecutable()
{
    static const QStringList terminalCandidates {
        "xdg-terminal-exec",
        "x-terminal-emulator",
        "kgx",
        "gnome-terminal",
        "konsole",
        "xfce4-terminal",
        "mate-terminal",
        "lxterminal",
        "tilix",
        "kitty",
        "alacritty",
        "urxvt",
        "terminator",
        "xterm"
    };
    static const QStringList trustedPrefixes {
        // FHS:
        "/usr/bin/",
        "/bin/",
        "/sbin/",
        "/usr/sbin/",
        // NixOS/Home-manager:
        "/nix/store/"
    };

    for (const QString &candidate : terminalCandidates)
    {
        const QString exe = QStandardPaths::findExecutable(candidate);
        if (!exe.isEmpty())
        {
            const QString canonical = QFileInfo(exe).canonicalFilePath();
            if (canonical.isEmpty())
                continue;

            for (const QString &prefix : trustedPrefixes)
            {
                if (canonical.startsWith(prefix))
                    return canonical;
            }
        }
    }

    return {};
}
