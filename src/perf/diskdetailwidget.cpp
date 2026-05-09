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

#include "diskdetailwidget.h"
#include "globals.h"
#include "metrics.h"
#include "ui_diskdetailwidget.h"
#include "../colorscheme.h"
#include "../misc.h"
#include "../ui/uihelper.h"
#include "../ui/widgetstyle.h"

#include <QGridLayout>
#include <QLabel>

using namespace Perf;

DiskDetailWidget::DiskDetailWidget(QWidget *parent) : QWidget(parent), ui(new Ui::DiskDetailWidget)
{
    this->ui->setupUi(this);
    const ColorScheme *scheme = ColorScheme::GetCurrent();

    WidgetStyle::ApplyTextStyle(this->ui->titleLabel, scheme->DiskTitleColor, 18, true);
    WidgetStyle::ApplyTextStyle(this->ui->modelLabel, scheme->DiskHeaderValueColor, 12);
    WidgetStyle::ApplyTextStyle(this->ui->activeGraphLabel, scheme->StatLabelColor, 8);
    WidgetStyle::ApplyTextStyle(this->ui->activeGraphMaxLabel, scheme->StatLabelColor, 8);
    WidgetStyle::ApplyTextStyle(this->ui->transferGraphLabel, scheme->StatLabelColor, 8);
    WidgetStyle::ApplyTextStyle(this->ui->transferGraphMaxLabel, scheme->StatLabelColor, 8);
    WidgetStyle::ApplyTextStyle(this->ui->activeTimeLeftLabel, scheme->AxisLabelColor, 8);
    WidgetStyle::ApplyTextStyle(this->ui->activeTimeRightLabel, scheme->AxisLabelColor, 8);
    WidgetStyle::ApplyTextStyle(this->ui->transferTimeLeftLabel, scheme->AxisLabelColor, 8);
    WidgetStyle::ApplyTextStyle(this->ui->transferTimeRightLabel, scheme->AxisLabelColor, 8);

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

    // Active time graph
    this->ui->activeGraphWidget->SetColor(scheme->DiskGraphLineColor, scheme->DiskGraphFillColor);
    this->ui->activeGraphWidget->SetSampleCapacity(TUX_MANAGER_HISTORY_SIZE);
    this->ui->activeGraphWidget->SetGridColumns(6);
    this->ui->activeGraphWidget->SetGridRows(4);
    this->ui->activeGraphWidget->SetSeriesNames(tr("Active time"));
    this->ui->activeGraphWidget->SetValueFormat(GraphWidget::ValueFormat::Percent);

    // Transfer graph (read + write overlay)
    this->ui->transferGraphWidget->SetColor(scheme->DiskTransferGraphLineColor, scheme->DiskTransferGraphFillColor, scheme->DiskTransferGraphSecondaryFillColor);
    this->ui->transferGraphWidget->SetSampleCapacity(TUX_MANAGER_HISTORY_SIZE);
    this->ui->transferGraphWidget->SetGridColumns(6);
    this->ui->transferGraphWidget->SetGridRows(4);
    this->ui->transferGraphWidget->SetSeriesNames(tr("Read"), tr("Write"));
    this->ui->transferGraphWidget->SetValueFormat(GraphWidget::ValueFormat::BytesPerSec);
    UIHelper::EnableGraphContextMenu(this->ui->activeGraphWidget);
    UIHelper::EnableGraphContextMenu(this->ui->transferGraphWidget);
    this->ui->activeGraphMaxLabel->setText(tr("100%"));
    UIHelper::EnableCopyLabelContextMenu(this->ui->activeValueLabel);
    UIHelper::EnableCopyLabelContextMenu(this->ui->capacityValueLabel);
    UIHelper::EnableCopyLabelContextMenu(this->ui->readValueLabel);
    UIHelper::EnableCopyLabelContextMenu(this->ui->formattedValueLabel);
    UIHelper::EnableCopyLabelContextMenu(this->ui->writeValueLabel);
    UIHelper::EnableCopyLabelContextMenu(this->ui->systemDiskValueLabel);
    UIHelper::EnableCopyLabelContextMenu(this->ui->deviceValueLabel);
    UIHelper::EnableCopyLabelContextMenu(this->ui->pageFileValueLabel);
    UIHelper::EnableCopyLabelContextMenu(this->ui->typeValueLabel);
}

DiskDetailWidget::~DiskDetailWidget()
{
    delete this->ui;
}

void DiskDetailWidget::ApplyColorScheme()
{
    const ColorScheme *scheme = ColorScheme::GetCurrent();
    WidgetStyle::ApplyTextStyle(this->ui->titleLabel, scheme->DiskTitleColor, 18, true);
    WidgetStyle::ApplyTextStyle(this->ui->modelLabel, scheme->DiskHeaderValueColor, 12);
    WidgetStyle::ApplyTextStyle(this->ui->activeGraphLabel, scheme->StatLabelColor, 8);
    WidgetStyle::ApplyTextStyle(this->ui->activeGraphMaxLabel, scheme->StatLabelColor, 8);
    WidgetStyle::ApplyTextStyle(this->ui->transferGraphLabel, scheme->StatLabelColor, 8);
    WidgetStyle::ApplyTextStyle(this->ui->transferGraphMaxLabel, scheme->StatLabelColor, 8);
    WidgetStyle::ApplyTextStyle(this->ui->activeTimeLeftLabel, scheme->AxisLabelColor, 8);
    WidgetStyle::ApplyTextStyle(this->ui->activeTimeRightLabel, scheme->AxisLabelColor, 8);
    WidgetStyle::ApplyTextStyle(this->ui->transferTimeLeftLabel, scheme->AxisLabelColor, 8);
    WidgetStyle::ApplyTextStyle(this->ui->transferTimeRightLabel, scheme->AxisLabelColor, 8);

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

    this->ui->activeGraphWidget->SetColor(scheme->DiskGraphLineColor, scheme->DiskGraphFillColor);
    this->ui->transferGraphWidget->SetColor(scheme->DiskTransferGraphLineColor, scheme->DiskTransferGraphFillColor, scheme->DiskTransferGraphSecondaryFillColor);
    this->update();
}

void DiskDetailWidget::SetDisk(int index)
{
    this->m_diskIndex = index;

    if (this->m_diskIndex >= 0 && this->m_diskIndex < Metrics::GetStorage()->DiskCount())
    {
        const Storage::DiskInfo &disk = Metrics::GetStorage()->FromIndex(this->m_diskIndex);

        this->ui->titleLabel->setText(tr("Disk (%1)").arg(disk.Name));
        this->ui->modelLabel->setText(disk.Model);
        this->ui->typeValueLabel->setText(disk.Type);
        this->ui->deviceValueLabel->setText("/dev/" + disk.Name);

        this->ui->activeGraphWidget->SetDataSource(disk.ActiveHistory, 100.0);
        this->ui->transferGraphWidget->SetDataSource(disk.ReadHistory, 1024.0);
        this->ui->transferGraphWidget->SetOverlayDataSource(disk.WriteHistory);
    }
    connect(Metrics::Get(), &Metrics::updated, this, &DiskDetailWidget::onUpdated);
    this->onUpdated();
}

void DiskDetailWidget::onUpdated()
{
    if (this->m_diskIndex < 0 || this->m_diskIndex >= Metrics::GetStorage()->DiskCount())
        return;

    const Storage::DiskInfo &disk = Metrics::GetStorage()->FromIndex(this->m_diskIndex);
    this->ui->activeValueLabel->setText(QString::number(disk.ActivePct, 'f', 0) + "%");
    this->ui->readValueLabel->setText(Misc::FormatBytesPerSecond(disk.ReadBps));
    this->ui->writeValueLabel->setText(Misc::FormatBytesPerSecond(disk.WriteBps));
    this->ui->capacityValueLabel->setText(Misc::FormatBytes(static_cast<quint64>(qMax<qint64>(0, disk.CapacityBytes)), 1));
    this->ui->formattedValueLabel->setText(disk.FormattedBytes > 0
                                           ? Misc::FormatBytes(static_cast<quint64>(qMax<qint64>(0, disk.FormattedBytes)), 1)
                                           : tr("-"));
    this->ui->systemDiskValueLabel->setText(disk.IsSystemDisk ? tr("Yes") : tr("No"));
    this->ui->pageFileValueLabel->setText(disk.HasPageFile ? tr("Yes") : tr("No"));

    this->ui->transferGraphWidget->SetMax(disk.MaxTransferBps);
    this->ui->activeGraphWidget->Tick();
    this->ui->transferGraphWidget->Tick();
    this->ui->transferGraphMaxLabel->setText(Misc::FormatBytesPerSecond(disk.MaxTransferBps));
}
