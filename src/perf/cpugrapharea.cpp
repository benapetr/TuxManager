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

#include "cpugrapharea.h"

#include "../colorscheme.h"
#include "configuration.h"
#include "graphwidget.h"
#include "perfdataprovider.h"

#include <QContextMenuEvent>
#include <QGridLayout>
#include <QStackedWidget>
#include <QVBoxLayout>

using namespace Perf;

CpuGraphArea::CpuGraphArea(QWidget *parent) : QWidget(parent), m_stack(new QStackedWidget(this))
{
    // Page 0 — single aggregate graph
    this->m_overallGraph = new GraphWidget(this->m_stack);
    this->m_overallGraph->SetSampleCapacity(CFG->PerfGraphWindowSec);
    this->m_overallGraph->SetSeriesNames(tr("CPU"), tr("Kernel"));
    this->m_overallGraph->SetValueFormat(GraphWidget::ValueFormat::Percent);
    this->m_stack->addWidget(this->m_overallGraph);  // index 0

    // Page 1 — per-core grid (populated lazily in ensureCoreGraphs)
    this->m_perCoreContainer = new QWidget(this->m_stack);
    this->m_perCoreGrid      = new QGridLayout(this->m_perCoreContainer);
    this->m_perCoreGrid->setSpacing(4);
    this->m_perCoreGrid->setContentsMargins(0, 0, 0, 0);
    this->m_perCoreContainer->setLayout(this->m_perCoreGrid);
    this->m_stack->addWidget(this->m_perCoreContainer);  // index 1

    QVBoxLayout *lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->addWidget(this->m_stack);
    this->setLayout(lay);

    this->m_stack->setCurrentIndex(0);
}

// ── Public interface ──────────────────────────────────────────────────────────

void CpuGraphArea::SetProvider(const PerfDataProvider *provider)
{
    this->m_provider = provider;
    if (!this->m_provider)
        return;

    this->bindOverallGraphSources();

    const int cores = this->m_provider->CoreCount();
    if (cores > 0)
    {
        this->ensureCoreGraphs(cores);
        this->bindCoreGraphSources(cores);
    }
}

void CpuGraphArea::SetMode(GraphMode mode)
{
    if (this->m_mode == mode)
        return;
    this->m_mode = mode;
    this->m_stack->setCurrentIndex(mode == GraphMode::Overall ? 0 : 1);
}

void CpuGraphArea::SetShowKernelTime(bool show)
{
    if (this->m_showKernelTime == show)
        return;
    this->m_showKernelTime = show;
    // When turning kernel-time OFF, immediately clear secondary histories so
    // the darker fill disappears without waiting for the next data tick.
    if (!show)
    {
        this->m_overallGraph->ClearOverlayDataSource();
        for (GraphWidget *g : this->m_coreGraphs)
            g->ClearOverlayDataSource();
        return;
    }

    this->bindOverallGraphSources();
    this->bindCoreGraphSources(this->m_coreGraphs.size());
}

void CpuGraphArea::UpdateData()
{
    if (!this->m_provider)
        return;

    // ── Aggregate graph ───────────────────────────────────────────────────────
    this->m_overallGraph->Tick();

    // ── Per-core grid ─────────────────────────────────────────────────────────
    const int cores = this->m_provider->CoreCount();
    if (cores > 0)
    {
        const bool coreGraphsRebuilt = (this->m_coreGraphs.size() != cores);
        this->ensureCoreGraphs(cores);
        if (coreGraphsRebuilt)
            this->bindCoreGraphSources(cores);

        for (int i = 0; i < cores; ++i)
        {
            GraphWidget *g = this->m_coreGraphs.at(i);
            if (!coreGraphsRebuilt)
                g->Tick();
            const double coreMhz = this->m_provider->CoreCurrentMhz(i);
            if (coreMhz > 0.0)
                g->SetOverlayText(tr("%1 GHz").arg(coreMhz / 1000.0, 0, 'f', 2));
            else
                g->SetOverlayText(QString());
        }
    }
}

void CpuGraphArea::ApplyColorScheme()
{
    const ColorScheme *scheme = ColorScheme::GetCurrent();
    this->m_overallGraph->SetColor(scheme->CpuGraphLineColor, scheme->CpuGraphFillColor, scheme->CpuGraphSecondaryFillColor);
    for (GraphWidget *g : this->m_coreGraphs)
    {
        if (g)
            g->SetColor(scheme->CpuGraphLineColor, scheme->CpuGraphFillColor, scheme->CpuGraphSecondaryFillColor);
    }
}

// ── Protected ─────────────────────────────────────────────────────────────────

void CpuGraphArea::contextMenuEvent(QContextMenuEvent *event)
{
    emit this->contextMenuRequested(event->globalPos());
    event->accept();
}

// ── Private ───────────────────────────────────────────────────────────────────

void CpuGraphArea::ensureCoreGraphs(int count)
{
    if (this->m_coreGraphs.size() == count)
        return;

    // Remove old widgets from the grid first
    while (QLayoutItem *item = this->m_perCoreGrid->takeAt(0))
    {
        if (QWidget *w = item->widget())
            w->deleteLater();
        delete item;
    }
    this->m_coreGraphs.clear();

    // Choose a column count that keeps the grid roughly square
    const int cols = (count <= 4) ? 2
                   : (count <= 9) ? 3
                   : 4;

    for (int i = 0; i < count; ++i)
    {
        GraphWidget *g = new GraphWidget(this->m_perCoreContainer);
        g->SetSampleCapacity(CFG->PerfGraphWindowSec);
        g->SetGridColumns(2);
        g->SetGridRows(2);
        g->SetSeriesNames(tr("CPU %1").arg(i), tr("Kernel"));
        g->SetValueFormat(GraphWidget::ValueFormat::Percent);
        g->setToolTip(tr("CPU %1").arg(i));
        g->show();   // explicitly unhide — parent may be on a hidden QStackedWidget page
        this->m_coreGraphs.append(g);
        this->m_perCoreGrid->addWidget(g, i / cols, i % cols);
    }

    // Equal stretching for rows and columns
    const int rows = (count + cols - 1) / cols;
    for (int r = 0; r < rows; ++r)
        this->m_perCoreGrid->setRowStretch(r, 1);
    for (int c = 0; c < cols; ++c)
        this->m_perCoreGrid->setColumnStretch(c, 1);

    // Notify the layout system that the container's size hint has changed
    this->m_perCoreContainer->updateGeometry();
}

void CpuGraphArea::bindOverallGraphSources()
{
    if (!this->m_provider)
        return;

    this->m_overallGraph->SetDataSource(this->m_provider->CpuHistory());
    if (this->m_showKernelTime)
        this->m_overallGraph->SetOverlayDataSource(this->m_provider->CpuKernelHistory());
    else
        this->m_overallGraph->ClearOverlayDataSource();
}

void CpuGraphArea::bindCoreGraphSources(int count)
{
    if (!this->m_provider)
        return;

    const int boundCount = qMin(count, this->m_coreGraphs.size());
    for (int i = 0; i < boundCount; ++i)
    {
        GraphWidget *g = this->m_coreGraphs.at(i);
        g->SetDataSource(this->m_provider->CoreHistory(i));
        if (this->m_showKernelTime)
            g->SetOverlayDataSource(this->m_provider->CoreKernelHistory(i));
        else
            g->ClearOverlayDataSource();
    }
}
