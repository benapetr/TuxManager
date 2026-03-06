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

#include "memorydetailwidget.h"
#include "ui_memorydetailwidget.h"

using namespace Perf;

MemoryDetailWidget::MemoryDetailWidget(QWidget *parent) : QWidget(parent), ui(new Ui::MemoryDetailWidget)
{
    this->ui->setupUi(this);

    // Memory graph: purple / magenta
    this->ui->graphWidget->SetColor(QColor(0xcc, 0x44, 0xcc), QColor(0x66, 0x11, 0x66, 130));
    this->ui->graphWidget->SetSampleCapacity(HISTORY_SIZE);
    this->ui->graphWidget->SetGridColumns(6);
    this->ui->graphWidget->SetGridRows(4);
    this->ui->graphWidget->SetSeriesNames(tr("Used memory"));
    this->ui->graphWidget->SetValueFormat(GraphWidget::ValueFormat::Percent);
}

MemoryDetailWidget::~MemoryDetailWidget()
{
    delete this->ui;
}

void MemoryDetailWidget::setProvider(PerfDataProvider *provider)
{
    if (this->m_provider)
        disconnect(this->m_provider, &PerfDataProvider::updated, this, &MemoryDetailWidget::onUpdated);

    this->m_provider = provider;

    if (this->m_provider)
    {
        connect(this->m_provider, &PerfDataProvider::updated, this, &MemoryDetailWidget::onUpdated);
        this->onUpdated();
    }
}

void MemoryDetailWidget::onUpdated()
{
    if (!this->m_provider)
        return;

    const qint64 total   = this->m_provider->MemTotalKb();
    const qint64 used    = this->m_provider->MemUsedKb();
    const qint64 avail   = this->m_provider->MemAvailKb();
    const qint64 free    = this->m_provider->MemFreeKb();
    const qint64 cached  = this->m_provider->MemCachedKb();   // includes buffers
    const qint64 buffers = this->m_provider->MemBuffersKb();
    const qint64 dirty   = this->m_provider->MemDirtyKb();

    this->ui->totalLabel->setText(fmtGb(total) + " GB");
    this->ui->statInUseValue->setText(fmtGb(used)    + " GB");
    this->ui->statAvailValue->setText(fmtGb(avail)   + " GB");
    this->ui->statCachedValue->setText(fmtGb(cached) + " GB");
    this->ui->statBuffersValue->setText(fmtGb(buffers) + " GB");
    this->ui->statFreeValue->setText(fmtGb(free)     + " GB");
    const int dimmUsed = this->m_provider->MemDimmSlotsUsed();
    const int dimmTotal = this->m_provider->MemDimmSlotsTotal();
    if (dimmTotal > 0)
        this->ui->statDimmSlotsValue->setText(tr("%1 / %2").arg(dimmUsed).arg(dimmTotal));
    else
        this->ui->statDimmSlotsValue->setText(tr("—"));

    const int memMtps = this->m_provider->MemSpeedMtps();
    if (memMtps > 0)
        this->ui->statMemSpeedValue->setText(tr("%1 MT/s").arg(memMtps));
    else
        this->ui->statMemSpeedValue->setText(tr("—"));

    // Dirty shown in MB when small, GB when large
    if (dirty < 1024LL * 1024LL)
        this->ui->statDirtyValue->setText(QString::number(dirty / 1024.0, 'f', 1) + " MB");
    else
        this->ui->statDirtyValue->setText(fmtGb(dirty) + " GB");

    // Composition bar — 4 segments must sum to total
    // free   = MemFree
    // cached = Buffers + PageCache  (includes dirty subset)
    // used   = Total - Free - Cached (htop formula, non-reclaimable)
    // Verify: used + cached + free == total  ✓
    this->ui->compositionBar->SetSegments(used, dirty, cached, free, total);

    this->ui->graphWidget->SetPercentTooltipAbsolute(static_cast<double>(total) / (1024.0 * 1024.0),
                                                     tr("GB"),
                                                     2);
    this->ui->graphWidget->SetHistory(this->m_provider->MemHistory());
}

// static
QString MemoryDetailWidget::fmtGb(qint64 kb)
{
    const double gb = static_cast<double>(kb) / (1024.0 * 1024.0);
    if (gb >= 10.0)
        return QString::number(gb, 'f', 1);
    return QString::number(gb, 'f', 2);
}
