#include "cpugrapharea.h"

#include "graphwidget.h"
#include "perfdataprovider.h"

#include <QContextMenuEvent>
#include <QGridLayout>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace Perf
{

CpuGraphArea::CpuGraphArea(QWidget *parent)
    : QWidget(parent)
    , m_stack(new QStackedWidget(this))
{
    // Page 0 — single aggregate graph
    this->m_overallGraph = new GraphWidget(this->m_stack);
    this->m_overallGraph->setSampleCapacity(HISTORY_SIZE);
    this->m_overallGraph->setSeriesNames(tr("CPU"), tr("Kernel"));
    this->m_overallGraph->setValueFormat(GraphWidget::ValueFormat::Percent);
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

void CpuGraphArea::setMode(GraphMode mode)
{
    if (this->m_mode == mode)
        return;
    this->m_mode = mode;
    this->m_stack->setCurrentIndex(mode == GraphMode::Overall ? 0 : 1);
}

void CpuGraphArea::setShowKernelTime(bool show)
{
    if (this->m_showKernelTime == show)
        return;
    this->m_showKernelTime = show;
    // When turning kernel-time OFF, immediately clear secondary histories so
    // the darker fill disappears without waiting for the next data tick.
    if (!show)
    {
        this->m_overallGraph->setSecondaryHistory({});
        for (GraphWidget *g : this->m_coreGraphs)
            g->setSecondaryHistory({});
    }
    // When turning ON the provider's next tick (≤1 s) will populate them.
}

void CpuGraphArea::updateData(const PerfDataProvider *provider)
{
    if (!provider)
        return;

    // ── Aggregate graph ───────────────────────────────────────────────────────
    this->m_overallGraph->setHistory(provider->cpuHistory());
    if (this->m_showKernelTime)
        this->m_overallGraph->setSecondaryHistory(provider->cpuKernelHistory());
    else
        this->m_overallGraph->setSecondaryHistory({});

    // ── Per-core grid ─────────────────────────────────────────────────────────
    const int cores = provider->coreCount();
    if (cores > 0)
    {
        this->ensureCoreGraphs(cores);
        for (int i = 0; i < cores; ++i)
        {
            this->m_coreGraphs.at(i)->setHistory(provider->coreHistory(i));
            if (this->m_showKernelTime)
                this->m_coreGraphs.at(i)->setSecondaryHistory(provider->coreKernelHistory(i));
            else
                this->m_coreGraphs.at(i)->setSecondaryHistory({});
        }
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
        g->setSampleCapacity(HISTORY_SIZE);
        g->setGridColumns(2);
        g->setGridRows(2);
        g->setSeriesNames(tr("CPU %1").arg(i), tr("Kernel"));
        g->setValueFormat(GraphWidget::ValueFormat::Percent);
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

} // namespace Perf
