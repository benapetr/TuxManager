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

#include "gpudetailwidget.h"
#include "configuration.h"
#include "../colorscheme.h"
#include "../misc.h"
#include "../ui/widgetstyle.h"

#include <algorithm>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QPalette>
#include <QVBoxLayout>

using namespace Perf;

namespace
{
    const HistoryBuffer kEmptyHistory;
}

GpuDetailWidget::GpuDetailWidget(QWidget *parent) : QWidget(parent)
{
    const ColorScheme *scheme = ColorScheme::GetCurrent();
    this->m_selectedEngineBySlot = CFG->GpuEngineSelectorIndices;
    while (this->m_selectedEngineBySlot.size() < 4)
        this->m_selectedEngineBySlot.append(this->m_selectedEngineBySlot.size());
    if (this->m_selectedEngineBySlot.size() > 4)
        this->m_selectedEngineBySlot.resize(4);

    auto configureGraph = [&](GraphWidget *graph)
    {
        graph->SetColor(scheme->GpuGraphLineColor, scheme->GpuGraphFillColor, scheme->GpuGraphSecondaryFillColor);
        graph->SetSampleCapacity(HISTORY_SIZE);
        graph->SetGridColumns(6);
        graph->SetGridRows(4);
        graph->SetValueFormat(GraphWidget::ValueFormat::Percent);
    };

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(12, 10, 12, 10);
    root->setSpacing(6);

    auto *header = new QHBoxLayout();
    this->m_titleLabel = new QLabel(tr("GPU"), this);
    QFont titleFont = this->m_titleLabel->font();
    titleFont.setPointSize(18);
    titleFont.setBold(true);
    this->m_titleLabel->setFont(titleFont);

    this->m_modelLabel = new QLabel(this);
    this->m_modelLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    QFont modelFont = this->m_modelLabel->font();
    modelFont.setPointSize(11);
    this->m_modelLabel->setFont(modelFont);

    header->addWidget(this->m_titleLabel, 1);
    header->addWidget(this->m_modelLabel, 1);
    root->addLayout(header);

    auto *graphsGrid = new QGridLayout();
    graphsGrid->setHorizontalSpacing(6);
    graphsGrid->setVerticalSpacing(6);

    for (int slot = 0; slot < 4; ++slot)
    {
        auto *cell = new QVBoxLayout();
        cell->setSpacing(2);

        auto *top = new QHBoxLayout();
        auto *selector = new QComboBox(this);
        auto *value = new QLabel("0%", this);
        value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        selector->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        top->addWidget(selector, 1);
        top->addWidget(value);

        auto *graph = new GraphWidget(this);
        configureGraph(graph);
        graph->setMinimumHeight(120);

        this->m_engineSelectors.append(selector);
        this->m_engineValueLabels.append(value);
        this->m_engineGraphs.append(graph);

        connect(selector,
                qOverload<int>(&QComboBox::currentIndexChanged),
                this,
                [this, slot](int index) { this->onEngineSelectionChanged(slot, index); });

        cell->addLayout(top);
        cell->addWidget(graph, 1);

        QWidget *host = new QWidget(this);
        host->setLayout(cell);
        graphsGrid->addWidget(host, slot / 2, slot % 2);
    }

    root->addLayout(graphsGrid, 1);

    auto *dedicatedHeader = new QHBoxLayout();
    dedicatedHeader->addWidget(new QLabel(tr("Dedicated GPU memory usage"), this), 1);
    this->m_dedicatedMemGraphMaxLabel = new QLabel(tr("0 GB"), this);
    this->m_dedicatedMemGraphMaxLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    dedicatedHeader->addWidget(this->m_dedicatedMemGraphMaxLabel);
    root->addLayout(dedicatedHeader);

    this->m_dedicatedMemGraph = new GraphWidget(this);
    configureGraph(this->m_dedicatedMemGraph);
    this->m_dedicatedMemGraph->SetSeriesNames(tr("Dedicated memory usage"));
    this->m_dedicatedMemGraph->setMinimumHeight(70);
    root->addWidget(this->m_dedicatedMemGraph);

    auto *dedicatedTimeAxis = new QHBoxLayout();
    dedicatedTimeAxis->addWidget(new QLabel(tr("60 seconds"), this));
    dedicatedTimeAxis->addStretch(1);
    dedicatedTimeAxis->addWidget(new QLabel(tr("0"), this));
    root->addLayout(dedicatedTimeAxis);

    auto *sharedHeader = new QHBoxLayout();
    sharedHeader->addWidget(new QLabel(tr("Shared GPU memory usage"), this), 1);
    this->m_sharedMemGraphMaxLabel = new QLabel(tr("0 GB"), this);
    this->m_sharedMemGraphMaxLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    sharedHeader->addWidget(this->m_sharedMemGraphMaxLabel);
    root->addLayout(sharedHeader);

    this->m_sharedMemGraph = new GraphWidget(this);
    configureGraph(this->m_sharedMemGraph);
    this->m_sharedMemGraph->SetSeriesNames(tr("Shared memory usage"));
    this->m_sharedMemGraph->setMinimumHeight(70);
    root->addWidget(this->m_sharedMemGraph);

    auto *sharedTimeAxis = new QHBoxLayout();
    sharedTimeAxis->addWidget(new QLabel(tr("60 seconds"), this));
    sharedTimeAxis->addStretch(1);
    sharedTimeAxis->addWidget(new QLabel(tr("0"), this));
    root->addLayout(sharedTimeAxis);

    auto *copyHeader = new QHBoxLayout();
    copyHeader->addWidget(new QLabel(tr("Copy bandwidth"), this));
    this->m_copyBwLegendLabel = new QLabel(tr("Light: TX  Dark: RX"), this);
    WidgetStyle::ApplyTextStyle(this->m_copyBwLegendLabel, scheme->StatLabelColor);
    this->m_copyBwLegendLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    copyHeader->addWidget(this->m_copyBwLegendLabel, 1);
    this->m_copyBwGraphMaxLabel = new QLabel(tr("0 KB/s"), this);
    this->m_copyBwGraphMaxLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    copyHeader->addWidget(this->m_copyBwGraphMaxLabel);
    root->addLayout(copyHeader);

    this->m_copyBwGraph = new GraphWidget(this);
    configureGraph(this->m_copyBwGraph);
    this->m_copyBwGraph->SetSeriesNames(tr("TX"), tr("RX"));
    this->m_copyBwGraph->SetValueFormat(GraphWidget::ValueFormat::BytesPerSec);
    this->m_copyBwGraph->setMinimumHeight(70);
    this->m_copyBwGraph->setToolTip(tr("Copy bandwidth: light trace = TX, dark trace = RX"));
    root->addWidget(this->m_copyBwGraph);

    auto *copyTimeAxis = new QHBoxLayout();
    copyTimeAxis->addWidget(new QLabel(tr("60 seconds"), this));
    copyTimeAxis->addStretch(1);
    copyTimeAxis->addWidget(new QLabel(tr("0"), this));
    root->addLayout(copyTimeAxis);

    auto *stats = new QGridLayout();
    stats->setHorizontalSpacing(20);
    stats->setVerticalSpacing(6);

    auto mkLabel = [this](const QString &text) -> QLabel *
    {
        Q_UNUSED(this);
        return new QLabel(text, this);
    };

    this->m_utilValueLabel = new QLabel("0%", this);
    this->m_tempValueLabel = new QLabel(tr("—"), this);
    this->m_gpuMemValueLabel = new QLabel("0 / 0 GB", this);
    this->m_dedicatedMemValueLabel = new QLabel("0 / 0 GB", this);
    this->m_sharedMemValueLabel = new QLabel("0 / 0 GB", this);
    this->m_driverValueLabel = new QLabel(tr("—"), this);
    this->m_backendValueLabel = new QLabel(tr("—"), this);

    stats->addWidget(mkLabel(tr("Utilization")), 0, 0);
    stats->addWidget(this->m_utilValueLabel, 0, 1);
    stats->addWidget(mkLabel(tr("Dedicated GPU memory")), 0, 2);
    stats->addWidget(this->m_dedicatedMemValueLabel, 0, 3);
    stats->addWidget(mkLabel(tr("GPU Memory")), 1, 0);
    stats->addWidget(this->m_gpuMemValueLabel, 1, 1);
    stats->addWidget(mkLabel(tr("Shared GPU memory")), 1, 2);
    stats->addWidget(this->m_sharedMemValueLabel, 1, 3);
    stats->addWidget(mkLabel(tr("Temperature")), 2, 0);
    stats->addWidget(this->m_tempValueLabel, 2, 1);
    stats->addWidget(mkLabel(tr("Driver version")), 2, 2);
    stats->addWidget(this->m_driverValueLabel, 2, 3);
    stats->addWidget(mkLabel(tr("Backend")), 3, 2);
    stats->addWidget(this->m_backendValueLabel, 3, 3);
    root->addLayout(stats);
}

void GpuDetailWidget::SetGpu(PerfDataProvider *provider, int index)
{
    if (this->m_provider)
        disconnect(this->m_provider, &PerfDataProvider::updated, this, &GpuDetailWidget::onUpdated);

    this->m_provider = provider;
    this->m_gpuIndex = index;
    this->m_dedicatedMemHistory = nullptr;
    this->m_sharedMemHistorySource = nullptr;
    this->m_copyTxHistory = nullptr;
    this->m_copyRxHistory = nullptr;

    if (this->m_provider)
    {
        connect(this->m_provider, &PerfDataProvider::updated, this, &GpuDetailWidget::onUpdated);
        if (this->m_gpuIndex >= 0 && this->m_gpuIndex < this->m_provider->GpuCount())
        {
            this->bindGpuIdentity();
            this->rebuildEngineSelectors();
            const bool hasSharedData = (this->m_provider->GpuSharedMemTotalMiB(this->m_gpuIndex) > 0);
            this->bindMemoryAndCopySources(hasSharedData);
            for (int slot = 0; slot < this->m_engineGraphs.size(); ++slot)
                this->bindEngineGraphSource(slot);
        }
        this->onUpdated();
    }
}

void GpuDetailWidget::ApplyColorScheme()
{
    const ColorScheme *scheme = ColorScheme::GetCurrent();
    WidgetStyle::ApplyTextStyle(this->m_copyBwLegendLabel, scheme->StatLabelColor);

    auto applyGraph = [scheme](GraphWidget *graph)
    {
        if (graph)
            graph->SetColor(scheme->GpuGraphLineColor, scheme->GpuGraphFillColor, scheme->GpuGraphSecondaryFillColor);
    };

    for (GraphWidget *graph : this->m_engineGraphs)
        applyGraph(graph);
    applyGraph(this->m_dedicatedMemGraph);
    applyGraph(this->m_sharedMemGraph);
    applyGraph(this->m_copyBwGraph);
    this->update();
}

void GpuDetailWidget::onEngineSelectionChanged(int slot, int comboIndex)
{
    if (slot < 0 || slot >= this->m_selectedEngineBySlot.size())
        return;

    if (slot < 0 || slot >= this->m_engineSelectors.size())
        return;

    QComboBox *combo = this->m_engineSelectors.at(slot);
    if (!combo || comboIndex < 0)
        return;

    this->m_selectedEngineBySlot[slot] = combo->itemData(comboIndex).toInt();
    CFG->GpuEngineSelectorIndices = this->m_selectedEngineBySlot;
    this->bindEngineGraphSource(slot);
    this->onUpdated();
}

void GpuDetailWidget::rebuildEngineSelectors()
{
    if (!this->m_provider || this->m_gpuIndex < 0 || this->m_gpuIndex >= this->m_provider->GpuCount())
        return;

    const int engineCount = this->m_provider->GpuEngineCount(this->m_gpuIndex);
    for (int slot = 0; slot < this->m_engineSelectors.size(); ++slot)
    {
        QComboBox *combo = this->m_engineSelectors.at(slot);
        if (!combo)
            continue;

        combo->blockSignals(true);
        combo->clear();

        for (int i = 0; i < engineCount; ++i)
            combo->addItem(this->m_provider->GpuEngineName(this->m_gpuIndex, i), i);

        int engineIndex = this->m_selectedEngineBySlot[slot];
        if (engineIndex < 0 || engineIndex >= engineCount)
            engineIndex = (engineCount > 0) ? (slot % engineCount) : -1;

        this->m_selectedEngineBySlot[slot] = engineIndex;
        if (engineIndex >= 0)
            combo->setCurrentIndex(combo->findData(engineIndex));

        combo->setEnabled(engineCount > 0);
        combo->blockSignals(false);
    }
}

void GpuDetailWidget::onUpdated()
{
    if (!this->m_provider || this->m_gpuIndex < 0 || this->m_gpuIndex >= this->m_provider->GpuCount())
        return;

    const double util = this->m_provider->GpuUtilPercent(this->m_gpuIndex);
    const int tempC = this->m_provider->GpuTemperatureC(this->m_gpuIndex);
    const qint64 dedicatedUsedMiB = this->m_provider->GpuMemUsedMiB(this->m_gpuIndex);
    const qint64 dedicatedTotalMiB = this->m_provider->GpuMemTotalMiB(this->m_gpuIndex);

    qint64 sharedTotalMiB = this->m_provider->GpuSharedMemTotalMiB(this->m_gpuIndex);
    qint64 sharedUsedMiB  = this->m_provider->GpuSharedMemUsedMiB(this->m_gpuIndex);
    const bool hasSharedData = (sharedTotalMiB > 0);
    if (!hasSharedData)
    {
        sharedTotalMiB = qMax<qint64>(0, this->m_provider->MemTotalKb() / 1024 / 2);
        sharedUsedMiB  = 0;
        this->m_sharedMemHistory.Push(0.0);
    }
    this->bindMemoryAndCopySources(hasSharedData);

    const qint64 gpuUsedMiB = dedicatedUsedMiB + sharedUsedMiB;
    const qint64 gpuTotalMiB = dedicatedTotalMiB + sharedTotalMiB;

    this->m_utilValueLabel->setText(QString::number(util, 'f', 0) + "%");
    this->m_tempValueLabel->setText(tempC >= 0 ? tr("%1 C").arg(tempC) : tr("—"));
    this->m_gpuMemValueLabel->setText(tr("%1 / %2")
                                      .arg(Misc::FormatMiB(static_cast<quint64>(qMax<qint64>(0, gpuUsedMiB)), 1))
                                      .arg(Misc::FormatMiB(static_cast<quint64>(qMax<qint64>(0, gpuTotalMiB)), 1)));
    this->m_dedicatedMemValueLabel->setText(tr("%1 / %2")
                                            .arg(Misc::FormatMiB(static_cast<quint64>(qMax<qint64>(0, dedicatedUsedMiB)), 1))
                                            .arg(Misc::FormatMiB(static_cast<quint64>(qMax<qint64>(0, dedicatedTotalMiB)), 1)));
    this->m_sharedMemValueLabel->setText(tr("%1 / %2")
                                         .arg(Misc::FormatMiB(static_cast<quint64>(qMax<qint64>(0, sharedUsedMiB)), 1))
                                         .arg(Misc::FormatMiB(static_cast<quint64>(qMax<qint64>(0, sharedTotalMiB)), 1)));
    this->m_driverValueLabel->setText(this->m_provider->GpuDriverVersion(this->m_gpuIndex));
    this->m_backendValueLabel->setText(this->m_provider->GpuBackendName(this->m_gpuIndex));

    // We assume that GPU engines don't change during runtime
    //this->rebuildEngineSelectors();

    for (int slot = 0; slot < this->m_engineGraphs.size(); ++slot)
    {
        const int engineIndex = (slot < this->m_selectedEngineBySlot.size())
                                ? this->m_selectedEngineBySlot.at(slot)
                                : -1;
        GraphWidget *graph = this->m_engineGraphs.at(slot);
        QLabel *value = this->m_engineValueLabels.at(slot);

        if (engineIndex >= 0)
        {
            value->setText(QString::number(this->m_provider->GpuEnginePercent(this->m_gpuIndex, engineIndex), 'f', 0) + "%");
        } else
        {
            value->setText("0%");
        }
        graph->Tick();
    }

    this->m_dedicatedMemGraph->SetPercentTooltipAbsolute(static_cast<double>(dedicatedTotalMiB) / 1024.0, tr("GB"), 2);
    this->m_dedicatedMemGraphMaxLabel->setText(Misc::FormatMiB(static_cast<quint64>(qMax<qint64>(0, dedicatedTotalMiB)), 1));
    this->m_dedicatedMemGraph->Tick();

    this->m_sharedMemGraph->SetPercentTooltipAbsolute(static_cast<double>(sharedTotalMiB) / 1024.0, tr("GB"), 2);
    this->m_sharedMemGraphMaxLabel->setText(Misc::FormatMiB(static_cast<quint64>(qMax<qint64>(0, sharedTotalMiB)), 1));
    this->m_sharedMemGraph->Tick();

    const double maxCopyRate = this->m_provider->GpuMaxCopyBytesPerSec(this->m_gpuIndex);
    this->m_copyBwGraph->SetMax(maxCopyRate);
    this->m_copyBwGraphMaxLabel->setText(Misc::FormatBytesPerSecond(maxCopyRate));
    this->m_copyBwGraph->Tick();
}

void GpuDetailWidget::bindGpuIdentity()
{
    if (!this->m_provider || this->m_gpuIndex < 0 || this->m_gpuIndex >= this->m_provider->GpuCount())
        return;

    this->m_titleLabel->setText(tr("GPU %1").arg(this->m_gpuIndex));
    this->m_modelLabel->setText(this->m_provider->GpuName(this->m_gpuIndex));
}

void GpuDetailWidget::bindEngineGraphSource(int slot)
{
    if (!this->m_provider || this->m_gpuIndex < 0 || this->m_gpuIndex >= this->m_provider->GpuCount())
        return;
    if (slot < 0 || slot >= this->m_engineGraphs.size())
        return;

    GraphWidget *graph = this->m_engineGraphs.at(slot);
    if (!graph)
        return;

    const int engineIndex = (slot < this->m_selectedEngineBySlot.size())
                            ? this->m_selectedEngineBySlot.at(slot)
                            : -1;
    if (engineIndex >= 0)
    {
        graph->SetSeriesNames(this->m_provider->GpuEngineName(this->m_gpuIndex, engineIndex));
        graph->SetDataSource(this->m_provider->GpuEngineHistory(this->m_gpuIndex, engineIndex), 100.0);
    } else
    {
        graph->SetSeriesNames(tr("Value"));
        graph->SetDataSource(kEmptyHistory, 100.0);
    }
}

void GpuDetailWidget::bindMemoryAndCopySources(bool hasSharedData)
{
    if (!this->m_provider || this->m_gpuIndex < 0 || this->m_gpuIndex >= this->m_provider->GpuCount())
        return;

    const HistoryBuffer *dedicatedMemHistory = &this->m_provider->GpuMemUsageHistory(this->m_gpuIndex);
    const HistoryBuffer *copyTxHistory = &this->m_provider->GpuCopyTxHistory(this->m_gpuIndex);
    const HistoryBuffer *copyRxHistory = &this->m_provider->GpuCopyRxHistory(this->m_gpuIndex);

    if (this->m_dedicatedMemHistory != dedicatedMemHistory)
    {
        this->m_dedicatedMemHistory = dedicatedMemHistory;
        this->m_dedicatedMemGraph->SetDataSource(*this->m_dedicatedMemHistory, 100.0);
    }

    if (this->m_copyTxHistory != copyTxHistory)
    {
        this->m_copyTxHistory = copyTxHistory;
        this->m_copyBwGraph->SetDataSource(*this->m_copyTxHistory, 1024.0);
    }

    if (this->m_copyRxHistory != copyRxHistory)
    {
        this->m_copyRxHistory = copyRxHistory;
        this->m_copyBwGraph->SetOverlayDataSource(*this->m_copyRxHistory);
    }

    const HistoryBuffer *desiredSharedHistory = hasSharedData
                                                  ? &this->m_provider->GpuSharedMemHistory(this->m_gpuIndex)
                                                  : &this->m_sharedMemHistory;
    if (this->m_sharedMemHistorySource != desiredSharedHistory)
    {
        this->m_sharedMemHistorySource = desiredSharedHistory;
        this->m_sharedMemGraph->SetDataSource(*this->m_sharedMemHistorySource, 100.0);
    }
}
