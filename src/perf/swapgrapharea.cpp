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

#include "swapgrapharea.h"

#include "../colorscheme.h"
#include "configuration.h"
#include "graphwidget.h"
#include "metrics.h"

#include <QContextMenuEvent>
#include <QFileInfo>
#include <QGridLayout>
#include <QStackedWidget>
#include <QVBoxLayout>

using namespace Perf;

SwapGraphArea::SwapGraphArea(QWidget *parent) : QWidget(parent), m_stack(new QStackedWidget(this))
{
    const ColorScheme *scheme = ColorScheme::GetCurrent();

    this->m_overallGraph = new GraphWidget(this->m_stack);
    this->m_overallGraph->SetSampleCapacity(CFG->PerfGraphWindowSec);
    this->m_overallGraph->SetGridColumns(6);
    this->m_overallGraph->SetGridRows(4);
    this->m_overallGraph->SetSeriesNames(tr("Swap usage"));
    this->m_overallGraph->SetValueFormat(GraphWidget::ValueFormat::Percent);
    this->m_overallGraph->SetColor(scheme->SwapUsageGraphLineColor, scheme->SwapUsageGraphFillColor);
    this->m_stack->addWidget(this->m_overallGraph);

    this->m_perDeviceContainer = new QWidget(this->m_stack);
    this->m_perDeviceGrid = new QGridLayout(this->m_perDeviceContainer);
    this->m_perDeviceGrid->setSpacing(4);
    this->m_perDeviceGrid->setContentsMargins(0, 0, 0, 0);
    this->m_perDeviceContainer->setLayout(this->m_perDeviceGrid);
    this->m_stack->addWidget(this->m_perDeviceContainer);

    auto *lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->addWidget(this->m_stack);
    this->setLayout(lay);

    this->m_stack->setCurrentIndex(0);
}

void SwapGraphArea::Init()
{
    this->bindOverallGraphSources();
    this->RebindDevices();
}

void SwapGraphArea::SetMode(GraphMode mode)
{
    if (this->m_mode == mode)
        return;
    this->m_mode = mode;
    this->m_stack->setCurrentIndex(mode == GraphMode::Overall ? 0 : 1);
}

void SwapGraphArea::ApplyColorScheme()
{
    const ColorScheme *scheme = ColorScheme::GetCurrent();
    this->m_overallGraph->SetColor(scheme->SwapUsageGraphLineColor, scheme->SwapUsageGraphFillColor);
    for (GraphWidget *g : std::as_const(this->m_deviceGraphs))
    {
        if (g)
            g->SetColor(scheme->SwapUsageGraphLineColor, scheme->SwapUsageGraphFillColor);
    }
}

void SwapGraphArea::UpdateData()
{
    const bool overallVisible = (this->m_mode == GraphMode::Overall);

    this->m_overallGraph->SetPercentTooltipAbsolute(static_cast<double>(Metrics::GetSwap()->SwapTotalKb()) / (1024.0 * 1024.0), tr("GB"), 2);
    this->m_overallGraph->Tick(overallVisible);

    const int deviceCount = Metrics::GetSwap()->SwapCount();
    if (this->m_deviceGraphs.size() != deviceCount)
        this->RebindDevices();

    const bool perDeviceVisible = !overallVisible;
    for (int i = 0; i < deviceCount && i < this->m_deviceGraphs.size(); ++i)
    {
        GraphWidget *g = this->m_deviceGraphs.at(i);
        if (!g)
            continue;
        const Swap::SwapInfo &swap = Metrics::GetSwap()->FromIndex(i);
        g->SetPercentTooltipAbsolute(static_cast<double>(swap.TotalKb) / (1024.0 * 1024.0), tr("GB"), 2);
        g->Tick(perDeviceVisible);
    }
}

void SwapGraphArea::RebindDevices()
{
    const int deviceCount = Metrics::GetSwap()->SwapCount();
    this->ensureDeviceGraphs(deviceCount);
    this->bindDeviceGraphSources(deviceCount);
}

void SwapGraphArea::contextMenuEvent(QContextMenuEvent *event)
{
    emit this->contextMenuRequested(event->globalPos());
    event->accept();
}

void SwapGraphArea::ensureDeviceGraphs(int count)
{
    if (this->m_deviceGraphs.size() == count)
        return;

    while (QLayoutItem *item = this->m_perDeviceGrid->takeAt(0))
    {
        if (QWidget *w = item->widget())
            w->deleteLater();
        delete item;
    }
    this->m_deviceGraphs.clear();

    if (count <= 0)
    {
        this->m_perDeviceContainer->updateGeometry();
        return;
    }

    const int cols = (count <= 4) ? 2
                   : (count <= 9) ? 3
                   : 4;

    for (int i = 0; i < count; ++i)
    {
        GraphWidget *g = new GraphWidget(this->m_perDeviceContainer);
        g->SetSampleCapacity(CFG->PerfGraphWindowSec);
        g->SetGridColumns(2);
        g->SetGridRows(2);
        g->SetValueFormat(GraphWidget::ValueFormat::Percent);
        g->SetColor(ColorScheme::GetCurrent()->SwapUsageGraphLineColor, ColorScheme::GetCurrent()->SwapUsageGraphFillColor);
        g->show();
        this->m_deviceGraphs.append(g);
        this->m_perDeviceGrid->addWidget(g, i / cols, i % cols);
    }

    const int rows = (count + cols - 1) / cols;
    for (int r = 0; r < rows; ++r)
        this->m_perDeviceGrid->setRowStretch(r, 1);
    for (int c = 0; c < cols; ++c)
        this->m_perDeviceGrid->setColumnStretch(c, 1);

    this->m_perDeviceContainer->updateGeometry();
}

void SwapGraphArea::bindOverallGraphSources()
{
    this->m_overallGraph->SetDataSource(Metrics::GetSwap()->SwapUsageHistory());
}

void SwapGraphArea::bindDeviceGraphSources(int count)
{
    const int boundCount = qMin(count, this->m_deviceGraphs.size());
    for (int i = 0; i < boundCount; ++i)
    {
        GraphWidget *g = this->m_deviceGraphs.at(i);
        const Swap::SwapInfo &swap = Metrics::GetSwap()->FromIndex(i);
        g->SetSeriesNames(swap.Name);
        g->setToolTip(swap.Name);
        const QString overlay = QFileInfo(swap.Name).fileName();
        g->SetOverlayText(overlay.isEmpty() ? swap.Name : overlay);
        g->SetDataSource(swap.UsageHistory);
        g->SetPercentTooltipAbsolute(static_cast<double>(swap.TotalKb) / (1024.0 * 1024.0), tr("GB"), 2);
    }
}
