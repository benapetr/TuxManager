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

#include "uihelper.h"
#include "../aboutdialog.h"
#include "../configuration.h"
#include "../metrics.h"
#include "../misc.h"
#include "../performancewidget.h"

#include <QAbstractItemModel>
#include <QAction>
#include <QActionGroup>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QMenu>
#include <QStringList>
#include <QScrollBar>
#include <QTableView>
#include <QTimer>
#include <QApplication>
#include <QClipboard>

QString UIHelper::GetVisibleRowText(const QTableView *view, int row)
{
    if (!view || !view->model() || row < 0)
        return QString();

    const QAbstractItemModel *model = view->model();
    const QHeaderView *header = view->horizontalHeader();
    QStringList parts;
    parts.reserve(model->columnCount());
    for (int col = 0; col < model->columnCount(); ++col)
    {
        if (header && header->isSectionHidden(col))
            continue;

        const QModelIndex idx = model->index(row, col);
        parts << model->data(idx, Qt::DisplayRole).toString();
    }
    return parts.join('\t');
}

QString UIHelper::GetVisibleRowsText(const QTableView *view, const QList<int> &rows)
{
    QStringList lines;
    lines.reserve(rows.size());
    for (int row : rows)
        lines << GetVisibleRowText(view, row);

    return lines.join('\n');
}

UIHelper::TableSelectionSnapshot UIHelper::CaptureTableSelection(const QTableView *view, int keyColumn, const std::function<QVariant(const QModelIndex &proxyKeyIndex)> &proxyKeyResolver)
{
    TableSelectionSnapshot out;
    if (!view || !view->model())
        return out;

    if (const QScrollBar *vsb = view->verticalScrollBar())
        out.ScrollPos = vsb->value();

    QItemSelectionModel *selectionModel = view->selectionModel();
    if (!selectionModel || !proxyKeyResolver)
        return out;

    const QModelIndexList selectedRows = selectionModel->selectedRows(keyColumn);
    for (const QModelIndex &proxyIdx : selectedRows)
    {
        const QVariant key = proxyKeyResolver(proxyIdx);
        if (!key.isNull() && !out.SelectedKeys.contains(key))
            out.SelectedKeys.append(key);
    }

    const QModelIndex currentProxy = view->currentIndex();
    if (currentProxy.isValid())
        out.CurrentKey = proxyKeyResolver(currentProxy.sibling(currentProxy.row(), keyColumn));

    return out;
}

void UIHelper::RestoreTableSelection(QTableView *view,
                           int keyColumn,
                           int sourceRowCount,
                           const std::function<QModelIndex(int sourceRow)> &sourceIndexForRow,
                           const std::function<QModelIndex(const QModelIndex &sourceIndex)> &sourceToProxy,
                           const std::function<QVariant(const QModelIndex &sourceKeyIndex)> &sourceKeyResolver,
                           const UIHelper::TableSelectionSnapshot &snapshot)
{
    if (!view || !view->model())
        return;

    if (!sourceIndexForRow || !sourceToProxy || !sourceKeyResolver)
        return;

    QItemSelectionModel *selectionModel = view->selectionModel();
    if (!selectionModel)
        return;

    selectionModel->clearSelection();
    QModelIndex restoredCurrentProxy;

    for (int row = 0; row < sourceRowCount; ++row)
    {
        const QModelIndex sourceKeyIdx = sourceIndexForRow(row);
        if (!sourceKeyIdx.isValid())
            continue;

        const QModelIndex proxyIdx = sourceToProxy(sourceKeyIdx);
        if (!proxyIdx.isValid())
            continue;

        const QVariant key = sourceKeyResolver(sourceKeyIdx);
        if (snapshot.SelectedKeys.contains(key))
            selectionModel->select(proxyIdx, QItemSelectionModel::Select | QItemSelectionModel::Rows);

        if (!snapshot.CurrentKey.isNull() && key == snapshot.CurrentKey)
            restoredCurrentProxy = proxyIdx;
    }

    if (restoredCurrentProxy.isValid())
    {
        selectionModel->setCurrentIndex(restoredCurrentProxy, QItemSelectionModel::NoUpdate);
    } else
    {
        const QModelIndexList selectedRows = selectionModel->selectedRows(keyColumn);
        if (!selectedRows.isEmpty())
            selectionModel->setCurrentIndex(selectedRows.first(), QItemSelectionModel::NoUpdate);
    }

    if (QScrollBar *vsb = view->verticalScrollBar())
        vsb->setValue(snapshot.ScrollPos);
}

void UIHelper::AddGlobalContextMenuItems(QMenu *menu, QWidget *parent)
{
    if (!menu)
        return;

    menu->addSeparator();
    QAction *aboutAction = menu->addAction(QObject::tr("About TuxManager"));
    QObject::connect(aboutAction, &QAction::triggered, menu, [menu, parent]()
    {
        QWidget *dialogParent = parent ? parent : qobject_cast<QWidget *>(menu->parent());
        AboutDialog dialog(dialogParent);
        dialog.exec();
    });
}

void UIHelper::EnableCopyLabelContextMenu(QLabel *label)
{
    if (!label)
        return;

    label->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(label, &QWidget::customContextMenuRequested, label, [label](const QPoint &pos)
    {
        QString text = label->text();
        text.replace('\r', ' ');
        text.replace('\n', ' ');
        QMenu menu(label);
        QAction *copyAction = menu.addAction(QObject::tr("Copy"));
        copyAction->setEnabled(!text.isEmpty() && text != QString::fromUtf8("—"));
        QObject::connect(copyAction, &QAction::triggered, &menu, [text]()
        {
            QApplication::clipboard()->setText(text);
        });
        menu.exec(label->mapToGlobal(pos));
    });

}

QAction *UIHelper::AddCopyWidgetAction(QMenu *menu, QWidget *widget, const QString &text)
{
    if (!menu || !widget)
        return nullptr;

    QAction *copyAction = menu->addAction(text.isEmpty() ? QObject::tr("Copy graph") : text);
    QObject::connect(copyAction, &QAction::triggered, menu, [widget]()
    {
        QApplication::clipboard()->setPixmap(widget->grab());
    });
    return copyAction;
}

void UIHelper::AddRefreshIntervalContextMenu(QMenu *menu)
{
    QMenu *refreshMenu = menu->addMenu(QObject::tr("Refresh interval"));
    refreshMenu->setObjectName("refreshIntervalMenu");
    auto *group = new QActionGroup(refreshMenu);
    for (int ms : CFG->RefreshRateAvailableIntervals)
    {
        QAction *action = refreshMenu->addAction(Misc::SimplifyTimeMS(ms));
        action->setCheckable(true);
        action->setData(ms);
        action->setChecked(!CFG->RefreshPaused && CFG->RefreshRateMs == ms);
        group->addAction(action);
    }
    refreshMenu->addSeparator();
    QAction *paused = refreshMenu->addAction(QObject::tr("Paused"));
    paused->setCheckable(true);
    paused->setData(0);
    paused->setChecked(CFG->RefreshPaused);
    group->addAction(paused);

    QObject::connect(refreshMenu, &QMenu::aboutToShow, refreshMenu, [group]()
    {
        const int current = CFG->RefreshPaused ? 0 : CFG->RefreshRateMs;
        for (QAction *action : group->actions())
            action->setChecked(action->data().toInt() == current);
    });
    QObject::connect(group, &QActionGroup::triggered, refreshMenu, [](QAction *action)
    {
        const int ms = action->data().toInt();
        CFG->RefreshPaused = ms == 0;
        if (ms > 0)
        {
            CFG->RefreshRateMs = ms;
            Metrics::Get()->SetInterval(ms);
        }
        emit CFG->RefreshSettingsChanged();
        if (PerformanceWidget *pw = PerformanceWidget::Get())
        {
            if (pw->IsActive())
                pw->onProviderUpdated();
        }
        CFG->Save();
    });
}

void UIHelper::AddGraphWindowContextMenu(QMenu *menu)
{
    QMenu *timeMenu = menu->addMenu(QObject::tr("Graph history"));
    timeMenu->setObjectName("graphHistoryMenu");
    auto *group = new QActionGroup(timeMenu);
    QVector<int> intervals = CFG->DataWindowAvailableIntervals;
    if (intervals.isEmpty())
        intervals.append(CFG->PerfGraphWindowSec);
    for (int sec : intervals)
    {
        QAction *action = timeMenu->addAction(Misc::SimplifyTime(sec));
        action->setCheckable(true);
        action->setData(sec);
        action->setChecked(CFG->PerfGraphWindowSec == sec);
        group->addAction(action);
    }
    QObject::connect(timeMenu, &QMenu::aboutToShow, timeMenu, [group]()
    {
        for (QAction *action : group->actions())
            action->setChecked(action->data().toInt() == CFG->PerfGraphWindowSec);
    });
    QObject::connect(group, &QActionGroup::triggered, timeMenu, [](QAction *action)
    {
        CFG->PerfGraphWindowSec = action->data().toInt();
        if (PerformanceWidget *pw = PerformanceWidget::Get())
        {
            pw->applyGraphWindowSeconds();
            if (pw->IsActive())
                pw->onProviderUpdated();
        }
        CFG->Save();
    });
}

void UIHelper::AddGraphContextMenuItems(QMenu *menu, QWidget *graphArea)
{
    AddRefreshIntervalContextMenu(menu);
    AddGraphWindowContextMenu(menu);

    menu->addSeparator();
    AddCopyWidgetAction(menu, graphArea, QObject::tr("Copy graph"));
}

void UIHelper::EnableGraphContextMenu(QWidget *widget)
{
    if (!widget)
        return;

    widget->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(widget, &QWidget::customContextMenuRequested, widget, [widget](const QPoint &pos)
    {
        QMenu menu(widget);
        AddGraphContextMenuItems(&menu, widget);
        menu.exec(widget->mapToGlobal(pos));
    });
}
