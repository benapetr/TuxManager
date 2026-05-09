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
#include "globals.h"
#include "metrics.h"
#include "ui_memorydetailwidget.h"
#include "../colorscheme.h"
#include "../misc.h"
#include "../ui/uihelper.h"
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
    WidgetStyle::ApplyTextStyle(this->ui->legendCompressedDot, scheme->MemoryLegendCompressedColor, 8);
    WidgetStyle::ApplyTextStyle(this->ui->legendCompressedLabel, scheme->MemoryLegendTextColor, 8);
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
    this->ui->graphWidget->SetSampleCapacity(TUX_MANAGER_HISTORY_SIZE);
    this->ui->graphWidget->SetGridColumns(6);
    this->ui->graphWidget->SetGridRows(4);
    this->ui->graphWidget->SetSeriesNames(tr("Used memory"));
    this->ui->graphWidget->SetValueFormat(GraphWidget::ValueFormat::Percent);
    UIHelper::EnableGraphContextMenu(this->ui->graphWidget);
    UIHelper::EnableCopyLabelContextMenu(this->ui->statInUseValue);
    UIHelper::EnableCopyLabelContextMenu(this->ui->statAvailValue);
    UIHelper::EnableCopyLabelContextMenu(this->ui->statCompressedValue);
    UIHelper::EnableCopyLabelContextMenu(this->ui->statDirtyValue);
    UIHelper::EnableCopyLabelContextMenu(this->ui->statFreeValue);
    UIHelper::EnableCopyLabelContextMenu(this->ui->statCachedValue);
    UIHelper::EnableCopyLabelContextMenu(this->ui->statBuffersValue);
    UIHelper::EnableCopyLabelContextMenu(this->ui->statDimmSlotsValue);
    UIHelper::EnableCopyLabelContextMenu(this->ui->statMemSpeedValue);

    this->updateCompressedVisibility(false);
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
    WidgetStyle::ApplyTextStyle(this->ui->legendCompressedDot, scheme->MemoryLegendCompressedColor, 8);
    WidgetStyle::ApplyTextStyle(this->ui->legendCompressedLabel, scheme->MemoryLegendTextColor, 8);
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

void MemoryDetailWidget::Init()
{
    this->m_memHistory = nullptr;

    const qint64 total = Metrics::GetMemory()->MemTotalKb();
    this->m_memHistory = &Metrics::GetMemory()->MemHistory();

    this->ui->totalLabel->setText(Misc::FormatKiB(static_cast<quint64>(qMax<qint64>(0, total)), 1));
    this->ui->graphWidget->SetPercentTooltipAbsolute(static_cast<double>(total) / (1024.0 * 1024.0), tr("GB"), 2);
    this->ui->graphWidget->SetDataSource(*this->m_memHistory);

    const int dimmUsed = Metrics::GetMemory()->MemDimmSlotsUsed();
    const int dimmTotal = Metrics::GetMemory()->MemDimmSlotsTotal();
    if (dimmTotal > 0)
        this->ui->statDimmSlotsValue->setText(tr("%1 / %2").arg(dimmUsed).arg(dimmTotal));
    else
        this->ui->statDimmSlotsValue->setText(tr("—"));

    const int memMtps = Metrics::GetMemory()->MemSpeedMtps();
    if (memMtps > 0)
        this->ui->statMemSpeedValue->setText(tr("%1 MT/s").arg(memMtps));
    else
        this->ui->statMemSpeedValue->setText(tr("—"));

    connect(Metrics::Get(), &Metrics::updated, this, &MemoryDetailWidget::onUpdated);
    this->onUpdated();
}

void MemoryDetailWidget::onUpdated()
{
    const qint64 total   = Metrics::GetMemory()->MemTotalKb();
    const qint64 used    = Metrics::GetMemory()->MemUsedNonZramKb();
    const qint64 avail   = Metrics::GetMemory()->MemAvailKb();
    const qint64 free    = Metrics::GetMemory()->MemFreeKb();
    const qint64 cached  = Metrics::GetMemory()->MemCachedKb();   // includes buffers
    const qint64 buffers = Metrics::GetMemory()->MemBuffersKb();
    const qint64 dirty   = Metrics::GetMemory()->MemDirtyKb();
    const qint64 compressedData = Metrics::GetMemory()->ZramCompressedKb();
    const qint64 compressedRam = Metrics::GetMemory()->ZramMemUsedKb();
    const bool hasZram = Metrics::GetMemory()->HasZram();

    this->ui->statInUseValue->setText(Misc::FormatKiB(static_cast<quint64>(qMax<qint64>(0, used)), 1));
    this->ui->statAvailValue->setText(Misc::FormatKiB(static_cast<quint64>(qMax<qint64>(0, avail)), 1));
    this->ui->statCachedValue->setText(Misc::FormatKiB(static_cast<quint64>(qMax<qint64>(0, cached)), 1));
    this->ui->statBuffersValue->setText(Misc::FormatKiB(static_cast<quint64>(qMax<qint64>(0, buffers)), 1));
    this->ui->statFreeValue->setText(Misc::FormatKiB(static_cast<quint64>(qMax<qint64>(0, free)), 1));
    this->ui->statDirtyValue->setText(Misc::FormatKiB(static_cast<quint64>(qMax<qint64>(0, dirty)), 1));
    if (hasZram)
    {
        this->ui->statCompressedValue->setText(tr("%1 (using %2 RAM)")
                                                   .arg(Misc::FormatKiB(static_cast<quint64>(qMax<qint64>(0, compressedData)), 1),
                                                        Misc::FormatKiB(static_cast<quint64>(qMax<qint64>(0, compressedRam)), 1)));
    } else
    {
        this->ui->statCompressedValue->setText(tr("—"));
    }
    this->updateCompressedVisibility(hasZram);

    // Composition bar — 5 segments must sum to total
    // free   = MemFree
    // cached = Buffers + PageCache  (includes dirty subset)
    // used   = Total - Free - Cached - zram_mem_used
    // zram   = RAM physically used by zram pools
    // Verify: used + zram + cached + free == total  ✓
    this->ui->compositionBar->SetSegments(used, hasZram ? compressedRam : 0, dirty, cached, free, total);

    if (this->m_memHistory)
        this->ui->graphWidget->Tick();
}

void MemoryDetailWidget::updateCompressedVisibility(bool visible)
{
    this->ui->legendCompressedDot->setVisible(visible);
    this->ui->legendCompressedLabel->setVisible(visible);
    this->ui->compressedLabel->setVisible(visible);
    this->ui->statCompressedValue->setVisible(visible);
}
