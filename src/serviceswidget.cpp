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

#include "serviceswidget.h"
#include "ui_serviceswidget.h"

#include "os/servicehelper.h"
#include "configuration.h"
#include "ui/uihelper.h"

#include <QClipboard>
#include <QHeaderView>
#include <QGuiApplication>
#include <QMenu>
#include <QMetaObject>
#include <QMessageBox>

#include <algorithm>

void ServiceRefreshWorker::fetch(quint64 token)
{
    QString reason;
    const bool systemdAvailable = OS::ServiceHelper::IsSystemdAvailable(&reason);
    QString error;
    QList<OS::Service> services;
    if (systemdAvailable)
        services = OS::Service::LoadAll(&error);

    emit fetched(token, systemdAvailable, reason, services, error);
}

ServicesWidget::ServicesWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ServicesWidget)
    , m_model(new OS::ServiceModel(this))
    , m_proxy(new OS::ServiceFilterProxy(this))
    , m_refreshTimer(new QTimer(this))
    , m_workerThread(new QThread(this))
    , m_worker(new ServiceRefreshWorker())
{
    this->ui->setupUi(this);
    qRegisterMetaType<OS::Service>("Os::Service");
    qRegisterMetaType<QList<OS::Service>>("QList<Os::Service>");

    this->m_proxy->setSourceModel(this->m_model);
    connect(this->ui->searchEdit, &QLineEdit::textChanged, this->m_proxy, &OS::ServiceFilterProxy::setFilterFixedString);

    this->ui->tableView->setModel(this->m_proxy);
    this->ui->tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    this->ui->tableView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    this->ui->tableView->setShowGrid(false);
    this->ui->tableView->setContextMenuPolicy(Qt::CustomContextMenu);
    this->ui->tableView->verticalHeader()->hide();

    QHeaderView *hv = this->ui->tableView->horizontalHeader();
    hv->setSectionsMovable(true);
    hv->setStretchLastSection(false);
    hv->setContextMenuPolicy(Qt::CustomContextMenu);
    hv->setSectionResizeMode(0, QHeaderView::Interactive);
    hv->setSectionResizeMode(1, QHeaderView::Interactive);
    hv->setSectionResizeMode(2, QHeaderView::Interactive);
    hv->setSectionResizeMode(3, QHeaderView::Interactive);
    hv->setSectionResizeMode(4, QHeaderView::Stretch);
    hv->setMinimumSectionSize(60);
    connect(hv, &QHeaderView::sectionMoved, this, [this]() { this->saveHeaderState(); });
    connect(hv, &QHeaderView::sectionResized, this, [this]() { this->saveHeaderState(); });
    this->ui->tableView->setColumnWidth(0, 260);
    this->ui->tableView->setColumnWidth(1, 90);
    this->ui->tableView->setColumnWidth(2, 90);
    this->ui->tableView->setColumnWidth(3, 100);
    this->ui->tableView->setSortingEnabled(true);
    this->ui->tableView->sortByColumn(0, Qt::AscendingOrder);
    if (!CFG->ServicesHeaderState.isEmpty())
        hv->restoreState(CFG->ServicesHeaderState);
    this->m_headerPersistenceEnabled = true;

    connect(hv, &QHeaderView::customContextMenuRequested, this, &ServicesWidget::onHeaderContextMenu);
    connect(this->ui->tableView, &QTableView::customContextMenuRequested, this, &ServicesWidget::onTableContextMenu);
    connect(this->m_refreshTimer, &QTimer::timeout, this, &ServicesWidget::onTimerTick);

    this->m_worker->moveToThread(this->m_workerThread);
    connect(this->m_workerThread, &QThread::finished, this->m_worker, &QObject::deleteLater);
    connect(this, &ServicesWidget::requestRefresh, this->m_worker, &ServiceRefreshWorker::fetch, Qt::QueuedConnection);
    connect(this->m_worker, &ServiceRefreshWorker::fetched, this, &ServicesWidget::onRefreshFinished, Qt::QueuedConnection);
    this->m_workerThread->start();
}

ServicesWidget::~ServicesWidget()
{
    this->m_refreshTimer->stop();
    this->m_workerThread->quit();
    this->m_workerThread->wait(1000);
    delete this->ui;
}

void ServicesWidget::SetActive(bool active)
{
    if (this->m_active == active)
        return;

    this->m_active = active;
    if (active)
    {
        this->startRefresh();
        this->m_refreshTimer->start(CFG->RefreshRateMs);
    } else
    {
        this->m_refreshTimer->stop();
        this->m_refreshPending = false;
    }
}

void ServicesWidget::onTimerTick()
{
    if (CFG->RefreshPaused)
        return;
    this->startRefresh();
}

void ServicesWidget::startRefresh()
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
    emit requestRefresh(this->m_refreshToken);
}

void ServicesWidget::onRefreshFinished(quint64 token, bool systemdAvailable, const QString &reason, const QList<OS::Service> &services, const QString &error)
{
    if (token != this->m_refreshToken)
        return;

    this->m_refreshInFlight = false;

    if (!this->m_active)
        return;

    if (this->m_tableContextMenuOpen)
    {
        this->m_refreshPending = true;
        return;
    }

    if (!systemdAvailable)
    {
        this->ui->tableView->setVisible(false);
        this->ui->unavailableLabel->setVisible(true);
        this->ui->unavailableLabel->setText(tr("systemd required (%1)").arg(reason));
        this->ui->statusLabel->setText(tr("Services unavailable"));
    } else if (!error.isEmpty())
    {
        this->ui->tableView->setVisible(false);
        this->ui->unavailableLabel->setVisible(true);
        this->ui->unavailableLabel->setText(tr("Failed to query services: %1").arg(error));
        this->ui->statusLabel->setText(tr("Services unavailable"));
    } else
    {
        const UIHelper::TableSelectionSnapshot snapshot = UIHelper::CaptureTableSelection(
            this->ui->tableView,
            OS::ServiceModel::ColService,
            [this](const QModelIndex &proxyKeyIndex) -> QVariant
            {
                const QModelIndex srcIdx = this->m_proxy->mapToSource(proxyKeyIndex);
                if (!srcIdx.isValid())
                    return {};
                return this->m_model->data(srcIdx, Qt::UserRole);
            });

        this->ui->unavailableLabel->setVisible(false);
        this->ui->tableView->setVisible(true);
        this->m_model->SetServices(services);
        this->ui->statusLabel->setText(tr("Services: %1").arg(services.size()));

        UIHelper::RestoreTableSelection(
            this->ui->tableView,
            OS::ServiceModel::ColService,
            this->m_model->rowCount(),
            [this](int row) -> QModelIndex { return this->m_model->index(row, OS::ServiceModel::ColService); },
            [this](const QModelIndex &sourceIndex) -> QModelIndex { return this->m_proxy->mapFromSource(sourceIndex); },
            [this](const QModelIndex &sourceKeyIndex) -> QVariant { return this->m_model->data(sourceKeyIndex, Qt::UserRole); },
            snapshot);
    }

    if (this->m_active && this->m_refreshPending && !CFG->RefreshPaused)
    {
        this->m_refreshPending = false;
        QMetaObject::invokeMethod(this, &ServicesWidget::startRefresh, Qt::QueuedConnection);
    }
}

void ServicesWidget::onHeaderContextMenu(const QPoint &pos)
{
    this->showHeaderContextMenu(this->ui->tableView->horizontalHeader(), OS::ServiceModel::ColCount, [this](int col)
    {
        return this->m_model->headerData(col, Qt::Horizontal).toString();
    }, pos);
}

void ServicesWidget::onTableContextMenu(const QPoint &pos)
{
    this->m_tableContextMenuOpen = true;

    const QModelIndex clickedIndex = this->ui->tableView->indexAt(pos);
    const QModelIndex targetIndex = clickedIndex.isValid() ? clickedIndex : this->ui->tableView->currentIndex();
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
    const QModelIndex sourceTargetIndex = this->sourceIndexFromProxy(targetIndex);
    const QString targetUnit = this->serviceUnitFromSourceIndex(sourceTargetIndex);
    const bool hasSingleTargetService = !targetUnit.isEmpty() && !multipleRowsSelected;

    QMenu menu(this);
    QMenu *copyMenu = menu.addMenu(tr("Copy"));

    QAction *copyRowAct = copyMenu->addAction(tr("Entire row"));
    copyRowAct->setEnabled(hasRowTarget);
    connect(copyRowAct, &QAction::triggered, this, [this, targetIndex, selectedRows, multipleRowsSelected]()
    {
        if (multipleRowsSelected)
        {
            QGuiApplication::clipboard()->setText(UIHelper::GetVisibleRowsText(this->ui->tableView, selectedRows));
            return;
        }

        const int row = targetIndex.isValid() ? targetIndex.row() : selectedRows.value(0, -1);
        QGuiApplication::clipboard()->setText(UIHelper::GetVisibleRowText(this->ui->tableView, row));
    });

    QAction *copyCellAct = copyMenu->addAction(tr("Selected cell"));
    copyCellAct->setEnabled(hasTargetCell && !multipleRowsSelected);
    connect(copyCellAct, &QAction::triggered, this, [this, targetIndex]()
    {
        QGuiApplication::clipboard()->setText(this->ui->tableView->model()->data(targetIndex, Qt::DisplayRole).toString());
    });

    menu.addSeparator();

    QAction *startAct = menu.addAction(tr("Start"));
    startAct->setEnabled(hasSingleTargetService);
    connect(startAct, &QAction::triggered, this, [this, targetUnit]()
    {
        this->manageServiceUnit(targetUnit, OS::ServiceHelper::UnitAction::Start, tr("start"));
    });

    QAction *stopAct = menu.addAction(tr("Stop"));
    stopAct->setEnabled(hasSingleTargetService);
    connect(stopAct, &QAction::triggered, this, [this, targetUnit]()
    {
        this->manageServiceUnit(targetUnit, OS::ServiceHelper::UnitAction::Stop, tr("stop"));
    });

    QAction *restartAct = menu.addAction(tr("Restart"));
    restartAct->setEnabled(hasSingleTargetService);
    connect(restartAct, &QAction::triggered, this, [this, targetUnit]()
    {
        this->manageServiceUnit(targetUnit, OS::ServiceHelper::UnitAction::Restart, tr("restart"));
    });

    QAction *reloadAct = menu.addAction(tr("Reload"));
    reloadAct->setEnabled(hasSingleTargetService);
    connect(reloadAct, &QAction::triggered, this, [this, targetUnit]()
    {
        this->manageServiceUnit(targetUnit, OS::ServiceHelper::UnitAction::Reload, tr("reload"));
    });

    QAction *tryRestartAct = menu.addAction(tr("Try Restart"));
    tryRestartAct->setEnabled(hasSingleTargetService);
    connect(tryRestartAct, &QAction::triggered, this, [this, targetUnit]()
    {
        this->manageServiceUnit(targetUnit, OS::ServiceHelper::UnitAction::TryRestart, tr("try restart"));
    });

    QAction *reloadOrRestartAct = menu.addAction(tr("Reload Or Restart"));
    reloadOrRestartAct->setEnabled(hasSingleTargetService);
    connect(reloadOrRestartAct, &QAction::triggered, this, [this, targetUnit]()
    {
        this->manageServiceUnit(targetUnit, OS::ServiceHelper::UnitAction::ReloadOrRestart, tr("reload or restart"));
    });

    menu.addSeparator();
    UIHelper::AddRefreshIntervalContextMenu(&menu, this->m_refreshTimer, this->m_active);
    UIHelper::AddGlobalContextMenuItems(&menu, this);

    menu.exec(this->ui->tableView->viewport()->mapToGlobal(pos));

    this->m_tableContextMenuOpen = false;
}

QModelIndex ServicesWidget::sourceIndexFromProxy(const QModelIndex &proxyIndex) const
{
    if (!proxyIndex.isValid())
        return {};
    return this->m_proxy->mapToSource(proxyIndex);
}

QString ServicesWidget::serviceUnitFromSourceIndex(const QModelIndex &sourceIndex) const
{
    if (!sourceIndex.isValid())
        return {};
    const QModelIndex unitIndex = sourceIndex.sibling(sourceIndex.row(), OS::ServiceModel::ColService);
    if (!unitIndex.isValid())
        return {};
    return this->m_model->data(unitIndex, Qt::UserRole).toString();
}

void ServicesWidget::manageServiceUnit(const QString &unit, OS::ServiceHelper::UnitAction action, const QString &actionLabel)
{
    if (unit.isEmpty())
        return;

    QString error;
    if (!OS::ServiceHelper::ManageUnit(unit, action, &error))
    {
        QMessageBox::warning(this, tr("Service action failed"), tr("Failed to %1 service %2.\n\n%3").arg(actionLabel, unit, error));
    }

    if (this->m_active && !CFG->RefreshPaused)
        this->startRefresh();
}

void ServicesWidget::showHeaderContextMenu(QHeaderView *header, int columnCount, const std::function<QString(int)> &titleForColumn, const QPoint &pos)
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
    if (chosen->isChecked())
        header->showSection(col);
    else
        header->hideSection(col);

    this->saveHeaderState();
}

void ServicesWidget::saveHeaderState() const
{
    if (!this->m_headerPersistenceEnabled)
        return;

    if (this->ui && this->ui->tableView && this->ui->tableView->horizontalHeader())
        CFG->ServicesHeaderState = this->ui->tableView->horizontalHeader()->saveState();
}
