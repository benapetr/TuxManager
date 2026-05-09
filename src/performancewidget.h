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

#ifndef PERFORMANCEWIDGET_H
#define PERFORMANCEWIDGET_H

#include "perf/sidepanel.h"
#include "perf/cpudetailwidget.h"
#include "perf/memorydetailwidget.h"
#include "perf/diskdetailwidget.h"
#include "perf/networkdetailwidget.h"
#include "perf/gpudetailwidget.h"
#include "perf/swapdetailwidget.h"

#include <QHash>
#include <QPoint>
#include <QSplitter>
#include <QStackedWidget>
#include <QVector>
#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui { class PerformanceWidget; }
QT_END_NAMESPACE

class PerformanceWidget : public QWidget
{
    Q_OBJECT

    public:
        explicit PerformanceWidget(QWidget *parent = nullptr);
        ~PerformanceWidget();
        void SetActive(bool active);
        bool IsActive() const { return this->m_active; }
        void ApplyColorScheme();

    private slots:
        void onProviderUpdated();
        void onSidePanelContextMenu(Perf::SidePanelItem *item, const QPoint &globalPos);

    private:
        Ui::PerformanceWidget      *ui;
        Perf::SidePanel            *m_sidePanel;
        QStackedWidget             *m_stack;
        QSplitter                  *m_splitter { nullptr };
        QHash<Perf::SidePanelItem *, QWidget *> m_detailByItem;
        Perf::SidePanelItem        *m_cpuItem { nullptr };
        Perf::SidePanelItem        *m_memoryItem { nullptr };
        Perf::SidePanelItem        *m_swapItem { nullptr };
        Perf::CpuDetailWidget      *m_cpuDetail;
        Perf::MemoryDetailWidget   *m_memDetail;
        Perf::SwapDetailWidget     *m_swapDetail;
        QVector<Perf::SidePanelItem *>   m_diskItems;
        QVector<Perf::DiskDetailWidget *> m_diskDetails;
        QVector<QString>                 m_diskNames;
        QVector<Perf::SidePanelItem *>   m_networkItems;
        QVector<Perf::NetworkDetailWidget *> m_networkDetails;
        QVector<QString>                 m_networkNames;
        QVector<Perf::SidePanelItem *>   m_gpuItems;
        QVector<Perf::GpuDetailWidget *> m_gpuDetails;
        QVector<QString>                 m_gpuNames;
        bool                             m_active { false };

        void setupLayout();
        void setupSidePanel();
        void setupDiskPanels();
        void setupNetworkPanels();
        void setupGpuPanels();
        void applySidePanelOrder();
        void tagTimeAxisLabels();
        void applyGraphWindowSeconds();
        void applyPanelVisibility();
        void applySidePanelGridEnabled();
        void updateSamplingPolicy();
};

#endif // PERFORMANCEWIDGET_H
