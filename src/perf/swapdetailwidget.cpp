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

#include "swapdetailwidget.h"
#include "ui_swapdetailwidget.h"
#include "../colorscheme.h"
#include "../misc.h"
#include "../ui/uihelper.h"
#include "../ui/widgetstyle.h"
#include "configuration.h"
#include "globals.h"
#include "metrics.h"

#include <QAction>
#include <QMenu>

using namespace Perf;

SwapDetailWidget::SwapDetailWidget(QWidget *parent) : QWidget(parent), ui(new Ui::SwapDetailWidget)
{
    this->ui->setupUi(this);

    const ColorScheme *scheme = ColorScheme::GetCurrent();
    WidgetStyle::ApplyTextStyle(this->ui->titleLabel, scheme->SwapUsageGraphLineColor, 18, true);
    WidgetStyle::ApplyTextStyle(this->ui->totalLabel, scheme->MutedTextColor, 11);

    WidgetStyle::ApplyTextStyle(this->ui->usageLabel, scheme->StatLabelColor, 8);
    WidgetStyle::ApplyTextStyle(this->ui->usageValueLabel, scheme->StatLabelColor, 8);
    WidgetStyle::ApplyTextStyle(this->ui->usageTimeLeftLabel, scheme->StatLabelColor, 8);
    WidgetStyle::ApplyTextStyle(this->ui->usageTimeRightLabel, scheme->StatLabelColor, 8);

    WidgetStyle::ApplyTextStyle(this->ui->activityLabel, scheme->StatLabelColor, 8);
    WidgetStyle::ApplyTextStyle(this->ui->activityMaxLabel, scheme->StatLabelColor, 8);
    WidgetStyle::ApplyTextStyle(this->ui->activityTimeLeftLabel, scheme->StatLabelColor, 8);
    WidgetStyle::ApplyTextStyle(this->ui->activityTimeRightLabel, scheme->StatLabelColor, 8);


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

    this->m_statLabels = {
        this->ui->usageLabel,
        this->ui->activityLabel,
        this->ui->inUseLabel,
        this->ui->freeLabel,
        this->ui->swapInLabel,
        this->ui->swapOutLabel
    };
    this->m_axisLabels = {
        this->ui->usageTimeLeftLabel,
        this->ui->usageTimeRightLabel,
        this->ui->activityTimeLeftLabel,
        this->ui->activityTimeRightLabel
    };

    this->ui->activityGraphWidget->SetColor(scheme->SwapActivityGraphLineColor, scheme->SwapActivityGraphFillColor, scheme->SwapActivityGraphSecondaryFillColor);
    this->ui->activityGraphWidget->SetSampleCapacity(TUX_MANAGER_HISTORY_SIZE);
    this->ui->activityGraphWidget->SetGridColumns(6);
    this->ui->activityGraphWidget->SetGridRows(4);
    this->ui->activityGraphWidget->SetSeriesNames(tr("Swap in"), tr("Swap out"));
    this->ui->activityGraphWidget->SetValueFormat(GraphWidget::ValueFormat::BytesPerSec);
    this->ui->activityGraphWidget->setToolTip(
                tr("Swap in: disk -> RAM (pages read back into memory)\n"
                   "Swap out: RAM -> disk (pages written to swap storage)"));
    UIHelper::EnableGraphContextMenu(this->ui->activityGraphWidget);

    UIHelper::EnableCopyLabelContextMenu(this->ui->usageValueLabel);
    UIHelper::EnableCopyLabelContextMenu(this->ui->inUseValueLabel);
    UIHelper::EnableCopyLabelContextMenu(this->ui->freeValueLabel);
    UIHelper::EnableCopyLabelContextMenu(this->ui->inRateValueLabel);
    UIHelper::EnableCopyLabelContextMenu(this->ui->outRateValueLabel);
}

SwapDetailWidget::~SwapDetailWidget()
{
    delete this->ui;
}

void SwapDetailWidget::Init()
{
    this->m_outHistory = &Metrics::GetSwap()->SwapOutHistory();
    this->m_inHistory = &Metrics::GetSwap()->SwapInHistory();

    connect(this->ui->usageGraphArea, &SwapGraphArea::contextMenuRequested, this, &SwapDetailWidget::onContextMenuRequested);

    this->ui->usageGraphArea->SetMode(
                CFG->SwapGraphMode == 1
                ? SwapGraphArea::GraphMode::PerDevice
                : SwapGraphArea::GraphMode::Overall);
    this->ui->usageGraphArea->Init();
    this->ui->activityGraphWidget->SetDataSource(*this->m_inHistory, 1024.0);
    this->ui->activityGraphWidget->SetOverlayDataSource(*this->m_outHistory);

    connect(Metrics::Get(), &Metrics::swapDevicesChanged, this, &SwapDetailWidget::onSwapDevicesChanged);
    connect(Metrics::Get(), &Metrics::updated, this, &SwapDetailWidget::onUpdated);
    this->onUpdated();
}

void SwapDetailWidget::ApplyColorScheme()
{
    const ColorScheme *scheme = ColorScheme::GetCurrent();
    WidgetStyle::ApplyTextStyle(this->ui->titleLabel, scheme->SwapUsageGraphLineColor, 18, true);
    WidgetStyle::ApplyTextStyle(this->ui->totalLabel, scheme->MutedTextColor, 11);

    WidgetStyle::ApplyTextStyle(this->ui->usageLabel, scheme->StatLabelColor, 8);
    WidgetStyle::ApplyTextStyle(this->ui->usageValueLabel, scheme->StatLabelColor, 8);
    WidgetStyle::ApplyTextStyle(this->ui->usageTimeLeftLabel, scheme->StatLabelColor, 8);
    WidgetStyle::ApplyTextStyle(this->ui->usageTimeRightLabel, scheme->StatLabelColor, 8);

    WidgetStyle::ApplyTextStyle(this->ui->activityLabel, scheme->StatLabelColor, 8);
    WidgetStyle::ApplyTextStyle(this->ui->activityMaxLabel, scheme->StatLabelColor, 8);
    WidgetStyle::ApplyTextStyle(this->ui->activityTimeLeftLabel, scheme->StatLabelColor, 8);
    WidgetStyle::ApplyTextStyle(this->ui->activityTimeRightLabel, scheme->StatLabelColor, 8);


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
    
    for (QLabel *label : std::as_const(this->m_statLabels))
        WidgetStyle::ApplyTextStyle(label, scheme->StatLabelColor);
    for (QLabel *label : std::as_const(this->m_axisLabels))
        WidgetStyle::ApplyTextStyle(label, scheme->AxisLabelColor);
    this->ui->usageGraphArea->ApplyColorScheme();
    this->ui->activityGraphWidget->SetColor(scheme->SwapActivityGraphLineColor, scheme->SwapActivityGraphFillColor, scheme->SwapActivityGraphSecondaryFillColor);
    this->update();
}

void SwapDetailWidget::onUpdated()
{
    const qint64 totalKb = Metrics::GetSwap()->SwapTotalKb();
    const qint64 usedKb = Metrics::GetSwap()->SwapUsedKb();
    const qint64 freeKb = Metrics::GetSwap()->SwapFreeKb();
    const double inBps = Metrics::GetSwap()->SwapInBytesPerSec();
    const double outBps = Metrics::GetSwap()->SwapOutBytesPerSec();

    const double usedPct = (totalKb > 0)
                           ? static_cast<double>(usedKb) * 100.0 / static_cast<double>(totalKb)
                           : 0.0;

    this->ui->totalLabel->setText(Misc::FormatKiB(static_cast<quint64>(qMax<qint64>(0, totalKb)), 1));
    this->ui->usageValueLabel->setText(QString::number(usedPct, 'f', 0) + "%");

    this->ui->inUseValueLabel->setText(Misc::FormatKiB(static_cast<quint64>(qMax<qint64>(0, usedKb)), 1));
    this->ui->freeValueLabel->setText(Misc::FormatKiB(static_cast<quint64>(qMax<qint64>(0, freeKb)), 1));
    this->ui->inRateValueLabel->setText(Misc::FormatBytesPerSecond(inBps));
    this->ui->outRateValueLabel->setText(Misc::FormatBytesPerSecond(outBps));

    const double maxRate = Metrics::GetSwap()->SwapMaxActivityBytesPerSec();
    this->ui->activityGraphWidget->SetMax(maxRate);

    this->ui->usageGraphArea->UpdateData();
    this->ui->activityGraphWidget->Tick();
    this->ui->activityMaxLabel->setText(Misc::FormatBytesPerSecond(maxRate));
}

void SwapDetailWidget::onContextMenuRequested(const QPoint &globalPos)
{
    QMenu menu(this);
    menu.setTitle(tr("Swap graph options"));

    QMenu *graphMenu = menu.addMenu(tr("Change graph to"));

    QAction *actOverall = graphMenu->addAction(tr("Overall usage"));
    QAction *actPerDevice = graphMenu->addAction(tr("Swap devices"));
    actOverall->setCheckable(true);
    actPerDevice->setCheckable(true);

    const bool isOverall = (this->ui->usageGraphArea->GetMode() == SwapGraphArea::GraphMode::Overall);
    actOverall->setChecked(isOverall);
    actPerDevice->setChecked(!isOverall);

    connect(actOverall, &QAction::triggered, this, [this]()
    {
        this->ui->usageGraphArea->SetMode(SwapGraphArea::GraphMode::Overall);
        CFG->SwapGraphMode = 0;
    });
    connect(actPerDevice, &QAction::triggered, this, [this]()
    {
        this->ui->usageGraphArea->SetMode(SwapGraphArea::GraphMode::PerDevice);
        CFG->SwapGraphMode = 1;
    });

    menu.addSeparator();

    UIHelper::AddGraphContextMenuItems(&menu, this->ui->usageGraphArea);

    menu.exec(globalPos);
}

void SwapDetailWidget::onSwapDevicesChanged()
{
    this->ui->usageGraphArea->RebindDevices();
}
