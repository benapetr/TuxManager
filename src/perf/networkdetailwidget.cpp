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

#include "networkdetailwidget.h"
#include "ui_networkdetailwidget.h"
#include "../colorscheme.h"
#include "../misc.h"
#include "../ui/widgetstyle.h"

#include <algorithm>
#include <QGridLayout>
#include <QLabel>

using namespace Perf;

NetworkDetailWidget::NetworkDetailWidget(QWidget *parent) : QWidget(parent), ui(new Ui::NetworkDetailWidget)
{
    this->ui->setupUi(this);
    const ColorScheme *scheme = ColorScheme::GetCurrent();

    WidgetStyle::ApplyTextStyle(this->ui->titleLabel, scheme->NetworkTitleColor, 18, true);
    WidgetStyle::ApplyTextStyle(this->ui->throughputGraphMaxLabel, scheme->StatLabelColor, 8);
    WidgetStyle::ApplyTextStyle(this->ui->throughputLabel, scheme->StatLabelColor, 8);
    WidgetStyle::ApplyTextStyle(this->ui->timeLeftLabel, scheme->AxisLabelColor, 8);
    WidgetStyle::ApplyTextStyle(this->ui->timeRightLabel, scheme->AxisLabelColor, 8);

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

    this->ui->throughputGraphWidget->SetColor(scheme->NetworkGraphLineColor, scheme->NetworkGraphFillColor, scheme->NetworkGraphSecondaryFillColor);
    this->ui->throughputGraphWidget->SetSampleCapacity(HISTORY_SIZE);
    this->ui->throughputGraphWidget->SetGridColumns(6);
    this->ui->throughputGraphWidget->SetGridRows(4);
    this->ui->throughputGraphWidget->SetSeriesNames(tr("Receive"), tr("Send"));
    this->ui->throughputGraphWidget->SetValueFormat(GraphWidget::ValueFormat::BytesPerSec);
}

NetworkDetailWidget::~NetworkDetailWidget()
{
    delete this->ui;
}

void NetworkDetailWidget::ApplyColorScheme()
{
    const ColorScheme *scheme = ColorScheme::GetCurrent();
    WidgetStyle::ApplyTextStyle(this->ui->titleLabel, scheme->NetworkTitleColor, 18, true);
    WidgetStyle::ApplyTextStyle(this->ui->throughputGraphMaxLabel, scheme->StatLabelColor, 8);
    WidgetStyle::ApplyTextStyle(this->ui->throughputLabel, scheme->StatLabelColor, 8);
    WidgetStyle::ApplyTextStyle(this->ui->timeLeftLabel, scheme->AxisLabelColor, 8);
    WidgetStyle::ApplyTextStyle(this->ui->timeRightLabel, scheme->AxisLabelColor, 8);

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

    this->ui->throughputGraphWidget->SetColor(scheme->NetworkGraphLineColor, scheme->NetworkGraphFillColor, scheme->NetworkGraphSecondaryFillColor);
    this->update();
}

void NetworkDetailWidget::SetNetwork(PerfDataProvider *provider, int index)
{
    if (this->m_provider)
    {
        disconnect(this->m_provider, &PerfDataProvider::updated, this, &NetworkDetailWidget::onUpdated);
    }

    this->m_provider = provider;
    this->m_networkIndex = index;

    if (this->m_provider)
    {
        if (this->m_networkIndex >= 0 && this->m_networkIndex < this->m_provider->NetworkCount())
        {
            const QString name = this->m_provider->NetworkName(this->m_networkIndex);
            const QString type = this->m_provider->NetworkType(this->m_networkIndex);
            const int speedMbps = this->m_provider->NetworkLinkSpeedMbps(this->m_networkIndex);
            const QString ipv4 = this->m_provider->NetworkIpv4(this->m_networkIndex);
            const QString ipv6 = this->m_provider->NetworkIpv6(this->m_networkIndex);

            this->m_rxHistory = &this->m_provider->NetworkRxHistory(this->m_networkIndex);
            this->m_txHistory = &this->m_provider->NetworkTxHistory(this->m_networkIndex);

            this->ui->titleLabel->setText(tr("NIC (%1)").arg(name));
            this->ui->adapterValueLabel->setText(name);
            this->ui->typeValueLabel->setText(type);
            this->ui->speedValueLabel->setText(speedMbps > 0 ? QString::number(speedMbps) + tr(" Mbps") : tr("Unknown"));
            this->ui->ipv4ValueLabel->setText(ipv4.isEmpty() ? tr("—") : ipv4);
            this->ui->ipv6ValueLabel->setText(ipv6.isEmpty() ? tr("—") : ipv6);

            this->ui->throughputGraphWidget->SetDataSource(*this->m_rxHistory, 1024.0);
            this->ui->throughputGraphWidget->SetOverlayDataSource(*this->m_txHistory);
        }
        connect(this->m_provider, &PerfDataProvider::updated, this, &NetworkDetailWidget::onUpdated);
        this->onUpdated();
    }
}

void NetworkDetailWidget::onUpdated()
{
    if (!this->m_rxHistory || !this->m_txHistory || !this->m_provider || this->m_networkIndex < 0 || this->m_networkIndex >= this->m_provider->NetworkCount())
        return;

    const double rxBps = this->m_provider->NetworkRxBytesPerSec(this->m_networkIndex);
    const double txBps = this->m_provider->NetworkTxBytesPerSec(this->m_networkIndex);
    this->ui->sendValueLabel->setText(Misc::FormatBytesPerSecond(txBps));
    this->ui->receiveValueLabel->setText(Misc::FormatBytesPerSecond(rxBps));

    const double maxRate = this->m_provider->NetworkMaxThroughputBytesPerSec(this->m_networkIndex);
    this->ui->throughputGraphWidget->SetMax(maxRate);
    this->ui->throughputGraphWidget->Tick();
    this->ui->throughputGraphMaxLabel->setText(Misc::FormatBytesPerSecond(maxRate));
}
