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
#include "../colorscheme.h"
#include "../misc.h"
#include "../ui/widgetstyle.h"

#include <QGridLayout>
#include <QLabel>

using namespace Perf;

MemoryDetailWidget::MemoryDetailWidget(QWidget *parent) : QWidget(parent), ui(new Ui::MemoryDetailWidget)
{
    this->ui->setupUi(this);
    const ColorScheme *scheme = ColorScheme::GetCurrent();

    WidgetStyle::ApplyTextStyle(this->ui->titleLabel, scheme->MemoryTitleColor, 18, true);
    WidgetStyle::ApplyTextStyle(this->ui->totalLabel, scheme->MemoryHeaderValueColor, 18);
    WidgetStyle::ApplyTextStyle(this->ui->timeLeftLabel, scheme->AxisLabelColor, 8);
    WidgetStyle::ApplyTextStyle(this->ui->timeRightLabel, scheme->AxisLabelColor, 8);
    WidgetStyle::ApplyTextStyle(this->ui->compositionLabel, scheme->StatLabelColor, 8);
    WidgetStyle::ApplyTextStyle(this->ui->legendUsedDot, scheme->MemoryLegendUsedColor, 8);
    WidgetStyle::ApplyTextStyle(this->ui->legendUsedLabel, scheme->MemoryLegendTextColor, 8);
    WidgetStyle::ApplyTextStyle(this->ui->legendDirtyDot, scheme->MemoryLegendDirtyColor, 8);
    WidgetStyle::ApplyTextStyle(this->ui->legendDirtyLabel, scheme->MemoryLegendTextColor, 8);
    WidgetStyle::ApplyTextStyle(this->ui->legendCachedDot, scheme->MemoryLegendCachedColor, 8);
    WidgetStyle::ApplyTextStyle(this->ui->legendCachedLabel, scheme->MemoryLegendTextColor, 8);
    WidgetStyle::ApplyTextStyle(this->ui->legendFreeDot, scheme->MemoryLegendFreeColor, 8);
    WidgetStyle::ApplyTextStyle(this->ui->legendFreeLabel, scheme->MemoryLegendTextColor, 8);

    if (QGridLayout *statsGrid = this->findChild<QGridLayout *>("statsGrid"))
    {
        for (int row = 0; row < statsGrid->rowCount(); ++row)
        {
            for (int column = 0; column < statsGrid->columnCount(); column += 2)
            {
                if (QLayoutItem *item = statsGrid->itemAtPosition(row, column))
                {
                    if (QLabel *label = qobject_cast<QLabel *>(item->widget()))
                        WidgetStyle::ApplyTextStyle(label, scheme->StatLabelColor);
                }
            }
        }
    }

    // Memory graph: purple / magenta
    this->ui->graphWidget->SetColor(scheme->MemoryGraphLineColor, scheme->MemoryGraphFillColor);
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

void MemoryDetailWidget::ApplyColorScheme()
{
    const ColorScheme *scheme = ColorScheme::GetCurrent();
    WidgetStyle::ApplyTextStyle(this->ui->titleLabel, scheme->MemoryTitleColor, 18, true);
    WidgetStyle::ApplyTextStyle(this->ui->totalLabel, scheme->MemoryHeaderValueColor, 18);
    WidgetStyle::ApplyTextStyle(this->ui->timeLeftLabel, scheme->AxisLabelColor, 8);
    WidgetStyle::ApplyTextStyle(this->ui->timeRightLabel, scheme->AxisLabelColor, 8);
    WidgetStyle::ApplyTextStyle(this->ui->compositionLabel, scheme->StatLabelColor, 8);
    WidgetStyle::ApplyTextStyle(this->ui->legendUsedDot, scheme->MemoryLegendUsedColor, 8);
    WidgetStyle::ApplyTextStyle(this->ui->legendUsedLabel, scheme->MemoryLegendTextColor, 8);
    WidgetStyle::ApplyTextStyle(this->ui->legendDirtyDot, scheme->MemoryLegendDirtyColor, 8);
    WidgetStyle::ApplyTextStyle(this->ui->legendDirtyLabel, scheme->MemoryLegendTextColor, 8);
    WidgetStyle::ApplyTextStyle(this->ui->legendCachedDot, scheme->MemoryLegendCachedColor, 8);
    WidgetStyle::ApplyTextStyle(this->ui->legendCachedLabel, scheme->MemoryLegendTextColor, 8);
    WidgetStyle::ApplyTextStyle(this->ui->legendFreeDot, scheme->MemoryLegendFreeColor, 8);
    WidgetStyle::ApplyTextStyle(this->ui->legendFreeLabel, scheme->MemoryLegendTextColor, 8);

    if (QGridLayout *statsGrid = this->findChild<QGridLayout *>("statsGrid"))
    {
        for (int row = 0; row < statsGrid->rowCount(); ++row)
        {
            for (int column = 0; column < statsGrid->columnCount(); column += 2)
            {
                if (QLayoutItem *item = statsGrid->itemAtPosition(row, column))
                {
                    if (QLabel *label = qobject_cast<QLabel *>(item->widget()))
                        WidgetStyle::ApplyTextStyle(label, scheme->StatLabelColor);
                }
            }
        }
    }

    this->ui->graphWidget->SetColor(scheme->MemoryGraphLineColor, scheme->MemoryGraphFillColor);
    this->ui->compositionBar->update();
    this->update();
}

void MemoryDetailWidget::setProvider(PerfDataProvider *provider)
{
    if (this->m_provider)
        disconnect(this->m_provider, &PerfDataProvider::updated, this, &MemoryDetailWidget::onUpdated);

    this->m_provider = provider;
    this->m_memHistory = nullptr;

    if (this->m_provider)
    {
        const qint64 total = this->m_provider->MemTotalKb();
        this->m_memHistory = &this->m_provider->MemHistory();

        this->ui->totalLabel->setText(Misc::FormatKiB(static_cast<quint64>(qMax<qint64>(0, total)), 1));
        this->ui->graphWidget->SetPercentTooltipAbsolute(static_cast<double>(total) / (1024.0 * 1024.0), tr("GB"), 2);
        this->ui->graphWidget->SetDataSource(*this->m_memHistory);

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

    this->ui->statInUseValue->setText(Misc::FormatKiB(static_cast<quint64>(qMax<qint64>(0, used)), 1));
    this->ui->statAvailValue->setText(Misc::FormatKiB(static_cast<quint64>(qMax<qint64>(0, avail)), 1));
    this->ui->statCachedValue->setText(Misc::FormatKiB(static_cast<quint64>(qMax<qint64>(0, cached)), 1));
    this->ui->statBuffersValue->setText(Misc::FormatKiB(static_cast<quint64>(qMax<qint64>(0, buffers)), 1));
    this->ui->statFreeValue->setText(Misc::FormatKiB(static_cast<quint64>(qMax<qint64>(0, free)), 1));
    this->ui->statDirtyValue->setText(Misc::FormatKiB(static_cast<quint64>(qMax<qint64>(0, dirty)), 1));

    // Composition bar — 4 segments must sum to total
    // free   = MemFree
    // cached = Buffers + PageCache  (includes dirty subset)
    // used   = Total - Free - Cached (htop formula, non-reclaimable)
    // Verify: used + cached + free == total  ✓
    this->ui->compositionBar->SetSegments(used, dirty, cached, free, total);

    if (this->m_memHistory)
        this->ui->graphWidget->Tick();
}
