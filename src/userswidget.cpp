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

#include "userswidget.h"
#include "ui_userswidget.h"

#include "configuration.h"
#include "misc.h"
#include "ui/uihelper.h"

#include <QHeaderView>
#include <QMetaObject>
#include <QMenu>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QTreeWidgetItem>

#include <unistd.h>

namespace
{
    struct UserAgg
    {
        QString name;
        QVector<const OS::Process *> procs;
        double cpuPct { 0.0 };
        quint64 memKb { 0 };
    };

    uid_t userId(const QTreeWidgetItem *item)
    {
        return item ? static_cast<uid_t>(item->data(0, Qt::UserRole).toUInt()) : 0;
    }

    pid_t processId(const QTreeWidgetItem *item)
    {
        return item ? static_cast<pid_t>(item->data(0, Qt::UserRole).toLongLong()) : 0;
    }

    void updateUserItem(QTreeWidgetItem *item, uid_t uid, const UserAgg &agg)
    {
        item->setText(0, QObject::tr("%1 (%2)").arg(agg.name).arg(agg.procs.size()));
        item->setData(0, Qt::UserRole, static_cast<uint>(uid));
        item->setText(1, QString::number(agg.cpuPct, 'f', 1) + "%");
        item->setText(2, Misc::FormatKiB(agg.memKb, 1));
        item->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
        item->setTextAlignment(2, Qt::AlignRight | Qt::AlignVCenter);
    }

    void updateProcessItem(QTreeWidgetItem *item, const OS::Process &proc)
    {
        item->setText(0, QObject::tr("%1 (pid %2)").arg(proc.Name).arg(proc.PID));
        item->setData(0, Qt::UserRole, static_cast<qlonglong>(proc.PID));
        item->setText(1, QString::number(proc.CPUPercent, 'f', 1) + "%");
        item->setText(2, Misc::FormatKiB(proc.VMRssKb, 1));
        item->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
        item->setTextAlignment(2, Qt::AlignRight | Qt::AlignVCenter);
    }
} // namespace

UsersWidget::UsersWidget(OS::ProcessRefreshService *processRefreshService, QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::UsersWidget)
    , m_processRefreshService(processRefreshService)
    , m_refreshTimer(new QTimer(this))
{
    this->ui->setupUi(this);

    this->ui->treeWidget->setColumnCount(3);
    this->ui->treeWidget->setHeaderLabels({ tr("User / Process"), tr("CPU"), tr("Memory") });
    this->ui->treeWidget->setRootIsDecorated(true);
    this->ui->treeWidget->setAlternatingRowColors(true);
    this->ui->treeWidget->setAnimated(false);
    this->ui->treeWidget->setContextMenuPolicy(Qt::CustomContextMenu);

    QHeaderView *hv = this->ui->treeWidget->header();
    hv->setSectionsClickable(true);
    hv->setSortIndicatorShown(true);
    hv->setSectionResizeMode(0, QHeaderView::Stretch);
    hv->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    hv->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    hv->setSortIndicator(this->m_sortColumn, this->m_sortOrder);

    connect(this->m_refreshTimer, &QTimer::timeout, this, &UsersWidget::onTimerTick);
    connect(this->m_processRefreshService,
            &OS::ProcessRefreshService::snapshotReady,
            this,
            &UsersWidget::onRefreshFinished);
    connect(this->ui->treeWidget, &QTreeWidget::customContextMenuRequested, this, &UsersWidget::onContextMenu);
    connect(hv, &QHeaderView::sortIndicatorChanged, this, [this](int column, Qt::SortOrder order)
    {
        this->m_sortColumn = column;
        this->m_sortOrder = order;
        if (this->m_active && !CFG->RefreshPaused)
            this->onTimerTick();
    });
}

UsersWidget::~UsersWidget()
{
    delete this->ui;
}

void UsersWidget::SetActive(bool active)
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

void UsersWidget::onTimerTick()
{
    if (CFG->RefreshPaused)
        return;
    this->startRefresh();
}

void UsersWidget::startRefresh()
{
    if (CFG->RefreshPaused)
    {
        this->m_refreshPending = false;
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

    OS::Process::LoadOptions opts;
    opts.IncludeKernelTasks = false;
    opts.IncludeOtherUsers = true;
    opts.MyUID = ::getuid();
    opts.CollectIOMetrics = false;
    opts.IsSuperuser = CFG->IsSuperuser;
    opts.EffectiveUID = CFG->EUID;

    this->m_processRefreshService->RequestSnapshot(OS::ProcessRefreshService::Consumer::Users,
                                                   this->m_refreshToken,
                                                   opts);
}

void UsersWidget::onRefreshFinished(int consumer, quint64 token, const QList<OS::Process> &processes)
{
    if (consumer != static_cast<int>(OS::ProcessRefreshService::Consumer::Users) || token != this->m_refreshToken)
        return;

    this->m_refreshInFlight = false;

    if (!this->m_active)
        return;

    this->rebuildTree(processes);

    if (this->m_active && this->m_refreshPending && !CFG->RefreshPaused)
    {
        this->m_refreshPending = false;
        QMetaObject::invokeMethod(this, &UsersWidget::startRefresh, Qt::QueuedConnection);
    }
}

void UsersWidget::onContextMenu(const QPoint &pos)
{
    QMenu menu(this);
    QHash<QAction *, int> refreshIntervalActions;
    QAction *pausedRefreshAction = nullptr;

    const QTreeWidgetItem *item = this->ui->treeWidget->itemAt(pos);
    const bool isProcessItem = item && item->parent();
    const pid_t selectedPid = isProcessItem
                              ? static_cast<pid_t>(item->data(0, Qt::UserRole).toLongLong())
                              : 0;

    if (isProcessItem && selectedPid > 0)
    {
        QAction *goToProcessAction = menu.addAction(tr("Go to process"));
        connect(goToProcessAction, &QAction::triggered, this, [this, selectedPid]()
        {
            emit goToProcessRequested(selectedPid);
        });
        menu.addSeparator();
    }

    QMenu *refreshMenu = menu.addMenu(tr("Refresh interval"));
    UIHelper::PopulateRefreshIntervalMenu(refreshMenu, refreshIntervalActions, pausedRefreshAction);
    UIHelper::AddGlobalContextMenuItems(&menu, this);

    QAction *picked = menu.exec(this->ui->treeWidget->viewport()->mapToGlobal(pos));
    UIHelper::ApplyRefreshIntervalAction(picked, refreshIntervalActions, pausedRefreshAction, this->m_refreshTimer, this->m_active);
}

void UsersWidget::rebuildTree(const QList<OS::Process> &allProcs)
{
    QSignalBlocker blocker(this->ui->treeWidget);
    QScrollBar *scrollbar = this->ui->treeWidget->verticalScrollBar();
    const int scrollPos = scrollbar ? scrollbar->value() : 0;

    QHash<uid_t, UserAgg> agg;
    for (const OS::Process &p : allProcs)
    {
        if (p.UID < 1000)
            continue;

        auto it = agg.find(p.UID);
        if (it == agg.end())
        {
            UserAgg a;
            a.name = p.User;
            it = agg.insert(p.UID, a);
        }

        it->procs.append(&p);
        it->cpuPct += p.CPUPercent;
        it->memKb += p.VMRssKb;
    }

    QHash<uid_t, QTreeWidgetItem *> userItems;
    for (int i = 0; i < this->ui->treeWidget->topLevelItemCount(); ++i)
    {
        if (QTreeWidgetItem *item = this->ui->treeWidget->topLevelItem(i))
            userItems.insert(userId(item), item);
    }

    QList<uid_t> uids = agg.keys();
    std::sort(uids.begin(), uids.end(), [&](uid_t a, uid_t b)
    {
        const UserAgg &left = agg[a];
        const UserAgg &right = agg[b];

        auto compare = [&](auto lhs, auto rhs)
        {
            if (lhs == rhs)
                return false;
            return (this->m_sortOrder == Qt::AscendingOrder) ? (lhs < rhs) : (lhs > rhs);
        };

        switch (this->m_sortColumn)
        {
            case 0:
                if (left.name != right.name)
                    return compare(left.name, right.name);
                break;
            case 1:
                if (!qFuzzyCompare(left.cpuPct + 1.0, right.cpuPct + 1.0))
                    return compare(left.cpuPct, right.cpuPct);
                break;
            case 2:
                if (left.memKb != right.memKb)
                    return compare(left.memKb, right.memKb);
                break;
            default:
                break;
        }

        if (!qFuzzyCompare(left.cpuPct + 1.0, right.cpuPct + 1.0))
            return left.cpuPct > right.cpuPct;
        if (left.memKb != right.memKb)
            return left.memKb > right.memKb;
        return left.name.localeAwareCompare(right.name) < 0;
    });

    for (int userIndex = 0; userIndex < uids.size(); ++userIndex)
    {
        const uid_t uid = uids.at(userIndex);
        const UserAgg &a = agg[uid];
        QTreeWidgetItem *userItem = userItems.take(uid);
        if (!userItem)
            userItem = new QTreeWidgetItem();

        updateUserItem(userItem, uid, a);

        if (this->ui->treeWidget->indexOfTopLevelItem(userItem) < 0)
        {
            this->ui->treeWidget->insertTopLevelItem(userIndex, userItem);
        } else
        {
            const int currentIndex = this->ui->treeWidget->indexOfTopLevelItem(userItem);
            if (currentIndex != userIndex)
            {
                this->ui->treeWidget->takeTopLevelItem(currentIndex);
                this->ui->treeWidget->insertTopLevelItem(userIndex, userItem);
            }
        }

        QHash<pid_t, QTreeWidgetItem *> processItems;
        for (int i = 0; i < userItem->childCount(); ++i)
        {
            if (QTreeWidgetItem *child = userItem->child(i))
                processItems.insert(processId(child), child);
        }

        QVector<const OS::Process *> procs = a.procs;
        std::sort(procs.begin(), procs.end(), [&](const OS::Process *lhs, const OS::Process *rhs)
        {
            const OS::Process &x = *lhs;
            const OS::Process &y = *rhs;
            auto compare = [&](auto lhs, auto rhs)
            {
                if (lhs == rhs)
                    return false;
                return (this->m_sortOrder == Qt::AscendingOrder) ? (lhs < rhs) : (lhs > rhs);
            };

            switch (this->m_sortColumn)
            {
                case 0:
                    if (x.Name != y.Name)
                        return compare(x.Name, y.Name);
                    break;
                case 1:
                    if (!qFuzzyCompare(x.CPUPercent + 1.0, y.CPUPercent + 1.0))
                        return compare(x.CPUPercent, y.CPUPercent);
                    break;
                case 2:
                    if (x.VMRssKb != y.VMRssKb)
                        return compare(x.VMRssKb, y.VMRssKb);
                    break;
                default:
                    break;
            }

            if (!qFuzzyCompare(x.CPUPercent + 1.0, y.CPUPercent + 1.0))
                return x.CPUPercent > y.CPUPercent;
            if (x.VMRssKb != y.VMRssKb)
                return x.VMRssKb > y.VMRssKb;
            if (x.Name != y.Name)
                return x.Name.localeAwareCompare(y.Name) < 0;
            return x.PID < y.PID;
        });

        for (int processIndex = 0; processIndex < procs.size(); ++processIndex)
        {
            const OS::Process &p = *procs.at(processIndex);
            QTreeWidgetItem *procItem = processItems.take(p.PID);
            if (!procItem)
                procItem = new QTreeWidgetItem();

            updateProcessItem(procItem, p);

            if (userItem->indexOfChild(procItem) < 0)
            {
                userItem->insertChild(processIndex, procItem);
            } else
            {
                const int currentIndex = userItem->indexOfChild(procItem);
                if (currentIndex != processIndex)
                {
                    userItem->takeChild(currentIndex);
                    userItem->insertChild(processIndex, procItem);
                }
            }
        }

        for (QTreeWidgetItem *staleProcessItem : std::as_const(processItems))
            delete staleProcessItem;
    }

    for (QTreeWidgetItem *staleUserItem : std::as_const(userItems))
    {
        const int index = this->ui->treeWidget->indexOfTopLevelItem(staleUserItem);
        if (index >= 0)
            delete this->ui->treeWidget->takeTopLevelItem(index);
    }

    if (scrollbar)
        scrollbar->setValue(scrollPos);

    this->ui->statusLabel->setText(tr("Logged in users: %1").arg(agg.size()));
}
