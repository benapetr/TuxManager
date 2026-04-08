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

#include "cpudetailwidget.h"
#include "ui_cpudetailwidget.h"
#include "configuration.h"
#include "../colorscheme.h"
#include "../ui/widgetstyle.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QFile>
#include <QGridLayout>
#include <QLabel>
#include <QMenu>
#include <QVBoxLayout>

using namespace Perf;

CpuDetailWidget::CpuDetailWidget(QWidget *parent) : QWidget(parent), ui(new Ui::CpuDetailWidget)
{
    this->ui->setupUi(this);
    const ColorScheme *scheme = ColorScheme::GetCurrent();

    WidgetStyle::ApplyTextStyle(this->ui->titleLabel, scheme->CpuTitleColor, 18, true);
    WidgetStyle::ApplyTextStyle(this->ui->modelNameLabel, scheme->MutedTextColor, 8);
    WidgetStyle::ApplyTextStyle(this->ui->utilizationLabel, scheme->CpuHeaderValueColor, 18);
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

    // Embed CpuGraphArea into the plain container widget from the .ui
    this->m_graphArea = new CpuGraphArea(this->ui->graphAreaContainer);
    QVBoxLayout *lay  = new QVBoxLayout(this->ui->graphAreaContainer);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->addWidget(this->m_graphArea);
    this->ui->graphAreaContainer->setLayout(lay);

    connect(this->m_graphArea, &CpuGraphArea::contextMenuRequested, this, &CpuDetailWidget::onContextMenuRequested);

    this->m_graphArea->SetMode(
                CFG->CpuGraphMode == 1
                ? CpuGraphArea::GraphMode::PerCore
                : CpuGraphArea::GraphMode::Overall);
    this->m_graphArea->SetShowKernelTime(CFG->CpuShowKernelTimes);
}

CpuDetailWidget::~CpuDetailWidget()
{
    delete this->ui;
}

void CpuDetailWidget::SetProvider(PerfDataProvider *provider)
{
    if (this->m_provider)
        disconnect(this->m_provider, &PerfDataProvider::updated, this, &CpuDetailWidget::onUpdated);

    this->m_provider = provider;

    if (this->m_provider)
    {
        // Populate one-time static labels from metadata
        this->ui->modelNameLabel->setText(this->m_provider->CpuModelName());
        this->ui->statLogicalCpusValue->setText(QString::number(this->m_provider->CpuLogicalCount()));
        this->m_graphArea->SetProvider(this->m_provider);

        connect(this->m_provider, &PerfDataProvider::updated, this, &CpuDetailWidget::onUpdated);
        this->onUpdated();
    }
}

void CpuDetailWidget::ApplyColorScheme()
{
    const ColorScheme *scheme = ColorScheme::GetCurrent();
    WidgetStyle::ApplyTextStyle(this->ui->titleLabel, scheme->CpuTitleColor, 18, true);
    WidgetStyle::ApplyTextStyle(this->ui->modelNameLabel, scheme->MutedTextColor, 8);
    WidgetStyle::ApplyTextStyle(this->ui->utilizationLabel, scheme->CpuHeaderValueColor, 18);
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

    this->m_graphArea->ApplyColorScheme();
    this->update();
}

// ── Private slots ─────────────────────────────────────────────────────────────

void CpuDetailWidget::onUpdated()
{
    if (!this->m_provider)
        return;

    const double pct = this->m_provider->CpuPercent();

    // Header utilisation
    this->ui->utilizationLabel->setText(QString::number(pct, 'f', 0) + "%");

    // Stats panel
    this->ui->statUtilValue->setText(QString::number(pct, 'f', 1) + "%");

    const double curMhz = this->m_provider->CpuCurrentMhz();
    if (curMhz > 0.0)
        this->ui->statSpeedValue->setText(
                tr("%1 GHz").arg(curMhz / 1000.0, 0, 'f', 2));
    else
        this->ui->statSpeedValue->setText(tr("—"));

    const int cpuTempC = this->m_provider->CpuTemperatureC();
    this->ui->statTempValue->setText(cpuTempC >= 0 ? tr("%1 C").arg(cpuTempC) : tr("—"));

    this->ui->statProcessesValue->setText(QString::number(this->m_provider->ProcessCount()));
    this->ui->statThreadsValue->setText(QString::number(this->m_provider->ThreadCount()));

    // Uptime from /proc/uptime
    QFile f("/proc/uptime");
    if (f.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        const double uptimeSec = f.readAll().simplified().split(' ').value(0).toDouble();
        f.close();
        const int days    = static_cast<int>(uptimeSec / 86400);
        const int hours   = static_cast<int>(uptimeSec / 3600)  % 24;
        const int minutes = static_cast<int>(uptimeSec / 60)    % 60;
        const int seconds = static_cast<int>(uptimeSec)         % 60;
        QString upStr;
        if (days > 0)
            upStr = tr("%1d %2:%3:%4")
                    .arg(days)
                    .arg(hours,   2, 10, QChar('0'))
                    .arg(minutes, 2, 10, QChar('0'))
                    .arg(seconds, 2, 10, QChar('0'));
        else
            upStr = tr("%1:%2:%3")
                    .arg(hours,   2, 10, QChar('0'))
                    .arg(minutes, 2, 10, QChar('0'))
                    .arg(seconds, 2, 10, QChar('0'));
        this->ui->statUptimeValue->setText(upStr);
    }

    // Update the graph area
    this->m_graphArea->UpdateData();

    if (this->m_provider->CpuIsVirtualMachine())
    {
        const QString vendor = this->m_provider->CpuVmVendor();
        if (vendor.isEmpty())
            this->ui->statVmValue->setText(tr("Yes"));
        else
            this->ui->statVmValue->setText(tr("Yes (%1)").arg(vendor));
    } else
    {
        this->ui->statVmValue->setText(tr("No"));
    }
}

void CpuDetailWidget::onContextMenuRequested(const QPoint &globalPos)
{
    QMenu menu(this);
    menu.setTitle(tr("CPU graph options"));

    // ── Change graph to ───────────────────────────────────────────────────────
    QMenu *graphMenu = menu.addMenu(tr("Change graph to"));

    QAction *actOverall  = graphMenu->addAction(tr("Overall utilization"));
    QAction *actPerCore  = graphMenu->addAction(tr("Logical processors"));
    actOverall->setCheckable(true);
    actPerCore->setCheckable(true);

    const bool isOverall = (this->m_graphArea->GetMode() == CpuGraphArea::GraphMode::Overall);
    actOverall->setChecked( isOverall);
    actPerCore->setChecked(!isOverall);

    connect(actOverall, &QAction::triggered, this, [this]()
    {
        this->m_graphArea->SetMode(CpuGraphArea::GraphMode::Overall);
        CFG->CpuGraphMode = 0;
    });
    connect(actPerCore, &QAction::triggered, this, [this]()
    {
        this->m_graphArea->SetMode(CpuGraphArea::GraphMode::PerCore);
        CFG->CpuGraphMode = 1;
    });

    menu.addSeparator();

    // ── Show kernel times ─────────────────────────────────────────────────────
    QAction *actKernel = menu.addAction(tr("Show kernel times"));
    actKernel->setCheckable(true);
    actKernel->setChecked(this->m_graphArea->GetShowKernelTime());
    connect(actKernel, &QAction::triggered, this, [this](bool checked)
    {
        this->m_graphArea->SetShowKernelTime(checked);
        CFG->CpuShowKernelTimes = checked;
    });

    menu.addSeparator();

    // ── Copy ─────────────────────────────────────────────────────────────────
    QAction *actCopy = menu.addAction(tr("Copy\tCtrl+C"));
    connect(actCopy, &QAction::triggered, this, [this]()
    {
        QApplication::clipboard()->setPixmap(this->m_graphArea->grab());
    });

    menu.exec(globalPos);
}
