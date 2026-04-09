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

#include "performancewidget.h"
#include "ui_performancewidget.h"

#include "colorschemedialog.h"
#include "configuration.h"
#include "colorscheme.h"
#include "logger.h"
#include "misc.h"
#include "perf/graphwidget.h"
#include "ui/uihelper.h"

#include <QAction>
#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPalette>
#include <QRegularExpression>

// ── Construction ──────────────────────────────────────────────────────────────

PerformanceWidget::PerformanceWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::PerformanceWidget)
    , m_provider(new Perf::PerfDataProvider(this))
    , m_sidePanel(new Perf::SidePanel(this))
    , m_stack(new QStackedWidget(this))
    , m_cpuDetail(new Perf::CpuDetailWidget(this))
    , m_memDetail(new Perf::MemoryDetailWidget(this))
    , m_swapDetail(new Perf::SwapDetailWidget(this))
{
    this->ui->setupUi(this);

    this->setupLayout();
    this->setupSidePanel();

    // Wire detail widgets to the data provider
    this->m_cpuDetail->SetProvider(this->m_provider);
    this->m_memDetail->setProvider(this->m_provider);
    this->m_swapDetail->SetProvider(this->m_provider);

    // Update side panel thumbnails on every sample
    connect(this->m_provider, &Perf::PerfDataProvider::updated, this, &PerformanceWidget::onProviderUpdated);

    // Expensive process/thread counting is only needed for CPU detail page.
    connect(this->m_sidePanel, &Perf::SidePanel::currentChanged, this, [this](int index)
    {
        this->m_provider->SetProcessStatsEnabled(index == this->m_cpuPanelIndex && CFG->PerfShowCpu);
    });
    connect(this->m_sidePanel, &Perf::SidePanel::itemContextMenuRequested, this, &PerformanceWidget::onSidePanelContextMenu);

    this->tagTimeAxisLabels();
    this->applyGraphWindowSeconds();
    this->applyPanelVisibility();
    this->updateSamplingPolicy();
    this->m_provider->SetProcessStatsEnabled(this->m_sidePanel->GetCurrentIndex() == this->m_cpuPanelIndex && CFG->PerfShowCpu);

    this->SetActive(false);

    LOG_DEBUG("PerformanceWidget initialised");
}

PerformanceWidget::~PerformanceWidget()
{
    delete this->ui;
}

// ── Private setup ─────────────────────────────────────────────────────────────

void PerformanceWidget::setupLayout()
{
    // The .ui gives us a bare QHBoxLayout (horizontalLayout) — populate it.
    QHBoxLayout *lay = qobject_cast<QHBoxLayout *>(this->layout());

    lay->addWidget(this->m_sidePanel);

    // Thin separator line
    QFrame *separator = new QFrame(this);
    separator->setFrameShape(QFrame::VLine);
    separator->setFrameShadow(QFrame::Sunken);
    lay->addWidget(separator);

    lay->addWidget(this->m_stack, /*stretch=*/1);
}

void PerformanceWidget::setupSidePanel()
{
    const ColorScheme *scheme = ColorScheme::GetCurrent();

    // ── CPU item ─────────────────────────────────────────────────────────────
    auto *cpuItem = new Perf::SidePanelItem(tr("CPU"), this);
    cpuItem->SetGraphColor(scheme->CpuGraphLineColor, scheme->CpuGraphFillColor);
    cpuItem->SetGraphSource(this->m_provider->CpuHistory());
    this->m_cpuPanelIndex = this->m_sidePanel->AddItem(cpuItem);
    this->m_stack->addWidget(this->m_cpuDetail);

    // ── Memory item ──────────────────────────────────────────────────────────
    auto *memItem = new Perf::SidePanelItem(tr("Memory"), this);
    memItem->SetGraphColor(scheme->MemoryGraphLineColor, scheme->MemoryGraphFillColor);
    memItem->SetGraphSource(this->m_provider->MemHistory());
    this->m_memoryPanelIndex = this->m_sidePanel->AddItem(memItem);
    this->m_stack->addWidget(this->m_memDetail);

    // ── Swap item ────────────────────────────────────────────────────────────
    auto *swapItem = new Perf::SidePanelItem(tr("Swap"), this);
    swapItem->SetGraphColor(scheme->SwapUsageGraphLineColor, scheme->SwapUsageGraphFillColor);
    swapItem->SetGraphSource(this->m_provider->SwapUsageHistory());
    this->m_swapPanelIndex = this->m_sidePanel->AddItem(swapItem);
    this->m_stack->addWidget(this->m_swapDetail);

    this->setupDiskPanels();
    this->setupNetworkPanels();
    this->setupGpuPanels();

    // Side-panel selection drives the stacked widget page
    connect(this->m_sidePanel, &Perf::SidePanel::currentChanged, this->m_stack, &QStackedWidget::setCurrentIndex);
}

void PerformanceWidget::setupDiskPanels()
{
    const ColorScheme *scheme = ColorScheme::GetCurrent();
    this->m_diskPanelStart = this->m_sidePanel->GetCount();
    const int count = this->m_provider->DiskCount();
    for (int i = 0; i < count; ++i)
    {
        const QString devName = this->m_provider->DiskName(i);
        this->m_diskNames.append(devName);

        auto *item = new Perf::SidePanelItem(tr("Disk (%1)").arg(devName), this);
        item->SetGraphColor(scheme->DiskGraphLineColor, scheme->DiskGraphFillColor);
        item->SetGraphSource(this->m_provider->DiskActiveHistory(i));
        this->m_sidePanel->AddItem(item);
        this->m_diskItems.append(item);

        auto *detail = new Perf::DiskDetailWidget(this);
        detail->SetDisk(this->m_provider, i);
        this->m_stack->addWidget(detail);
        this->m_diskDetails.append(detail);
    }
}

void PerformanceWidget::setupGpuPanels()
{
    const ColorScheme *scheme = ColorScheme::GetCurrent();
    this->m_gpuPanelStart = this->m_sidePanel->GetCount();
    const int count = this->m_provider->GpuCount();
    for (int i = 0; i < count; ++i)
    {
        const QString GpuName = this->m_provider->GpuName(i);
        this->m_gpuNames.append(GpuName);

        auto *item = new Perf::SidePanelItem(tr("GPU %1").arg(i), this);
        item->SetGraphColor(scheme->GpuGraphLineColor, scheme->GpuGraphFillColor);
        item->SetGraphSource(this->m_provider->GpuUtilHistory(i));
        this->m_sidePanel->AddItem(item);
        this->m_gpuItems.append(item);

        auto *detail = new Perf::GpuDetailWidget(this);
        detail->SetGpu(this->m_provider, i);
        this->m_stack->addWidget(detail);
        this->m_gpuDetails.append(detail);
    }
}

void PerformanceWidget::setupNetworkPanels()
{
    const ColorScheme *scheme = ColorScheme::GetCurrent();
    this->m_networkPanelStart = this->m_sidePanel->GetCount();
    const int count = this->m_provider->NetworkCount();
    for (int i = 0; i < count; ++i)
    {
        const QString ifName = this->m_provider->NetworkName(i);
        this->m_networkNames.append(ifName);

        auto *item = new Perf::SidePanelItem(tr("NIC (%1)").arg(ifName), this);
        item->SetGraphColor(scheme->NetworkGraphLineColor, scheme->NetworkGraphFillColor);
        item->SetGraphSource(this->m_provider->NetworkRxHistory(i), 1024.0);
        this->m_sidePanel->AddItem(item);
        this->m_networkItems.append(item);

        auto *detail = new Perf::NetworkDetailWidget(this);
        detail->SetNetwork(this->m_provider, i);
        this->m_stack->addWidget(detail);
        this->m_networkDetails.append(detail);
    }
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void PerformanceWidget::onProviderUpdated()
{
    // Update CPU side panel item
    const double cpuPct = this->m_provider->CpuPercent();
    const int cpuTempC = this->m_provider->CpuTemperatureC();
    const QString cpuSub = (cpuTempC >= 0)
                           ? tr("%1%2 %3C", "%1=value %2=percent sign %3=temperature in Celsius")
                                 .arg(QString::number(cpuPct, 'f', 0), "%", QString::number(cpuTempC))
                           : QString::number(cpuPct, 'f', 0) + "%";
    if (CFG->PerfShowCpu)
    {
        if (auto *item = this->m_sidePanel->GetItemAt(this->m_cpuPanelIndex))
            item->Update(cpuSub);
    }

    // Update Memory side panel item
    const qint64 used  = this->m_provider->MemUsedKb();
    const qint64 total = this->m_provider->MemTotalKb();
    const int    pct     = total > 0
                           ? static_cast<int>(static_cast<double>(used) / total * 100.0)
                           : 0;
    const QString memSub = QString("%1/%2 (%3%)")
                           .arg(Misc::FormatKiB(static_cast<quint64>(qMax<qint64>(0, used)), 1),
                                Misc::FormatKiB(static_cast<quint64>(qMax<qint64>(0, total)), 1),
                                QString::number(pct));
    if (CFG->PerfShowMemory)
    {
        if (auto *item = this->m_sidePanel->GetItemAt(this->m_memoryPanelIndex))
            item->Update(memSub);
    }

    // Update Swap side panel item
    const qint64 swapUsed = this->m_provider->SwapUsedKb();
    const qint64 swapTotal = this->m_provider->SwapTotalKb();
    const int swapPct = (swapTotal > 0)
                        ? static_cast<int>(static_cast<double>(swapUsed) / static_cast<double>(swapTotal) * 100.0)
                        : 0;
    QString swapSub;
    if (swapTotal > 0)
        swapSub = QString("%1/%2 (%3%)")
                  .arg(Misc::FormatKiB(static_cast<quint64>(qMax<qint64>(0, swapUsed)), 1),
                       Misc::FormatKiB(static_cast<quint64>(qMax<qint64>(0, swapTotal)), 1),
                       QString::number(swapPct));
    else
        swapSub = tr("Off");
    if (CFG->PerfShowSwap)
    {
        if (auto *item = this->m_sidePanel->GetItemAt(this->m_swapPanelIndex))
            item->Update(swapSub);
    }

    if (CFG->PerfShowDisks)
    {
        for (int i = 0; i < this->m_diskItems.size(); ++i)
        {
            if (i >= this->m_provider->DiskCount())
                break;
            auto *item = this->m_diskItems.at(i);
            if (!item)
                continue;

            const QString diskSub = tr("%1 %2", "%1=disk type %2=active percentage")
                                    .arg(this->m_provider->DiskType(i),
                                         QString::number(this->m_provider->DiskActivePercent(i), 'f', 0) + "%");
            item->Update(diskSub);
        }
    }

    if (CFG->PerfShowGpu)
    {
        for (int i = 0; i < this->m_gpuItems.size(); ++i)
        {
            if (i >= this->m_provider->GpuCount())
                break;
            auto *item = this->m_gpuItems.at(i);
            if (!item)
                continue;

            const QString utilText = tr("%1%2", "%1=GPU utilization value %2=percent sign")
                                     .arg(QString::number(this->m_provider->GpuUtilPercent(i), 'f', 0), "%");
            const int tempC = this->m_provider->GpuTemperatureC(i);
            const QString sub = (tempC >= 0)
                                ? tr("%1 %2C", "%1=GPU utilization %2=temperature in Celsius")
                                      .arg(utilText, QString::number(tempC))
                                : utilText;
            item->Update(sub);
        }
    }

    if (CFG->PerfShowNetwork)
    {
        for (int i = 0; i < this->m_networkItems.size(); ++i)
        {
            if (i >= this->m_provider->NetworkCount())
                break;
            auto *item = this->m_networkItems.at(i);
            if (!item)
                continue;

            const double tx = this->m_provider->NetworkTxBytesPerSec(i);
            const double rx = this->m_provider->NetworkRxBytesPerSec(i);
            const QString netSub = tr("U:%1 D:%2", "%1=upload rate %2=download rate")
                                   .arg(Misc::FormatBytesPerSecond(tx), Misc::FormatBytesPerSecond(rx));
            const double maxRate = this->m_provider->NetworkMaxThroughputBytesPerSec(i);
            item->Update(netSub, maxRate);
        }
    }
}

void PerformanceWidget::SetActive(bool active)
{
    if (this->m_active == active)
        return;

    this->m_active = active;
    this->m_provider->SetActive(active);
    if (active)
        this->onProviderUpdated();
}

void PerformanceWidget::onSidePanelContextMenu(int /*index*/, const QPoint &globalPos)
{
    QMenu menu(this);
    QHash<QAction *, int> graphWindowActions;
    QHash<QAction *, int> refreshIntervalActions;
    QAction *pausedRefreshAction = nullptr;

    QAction *cpu = menu.addAction(tr("CPU"));
    cpu->setCheckable(true);
    cpu->setChecked(CFG->PerfShowCpu);

    QAction *memory = menu.addAction(tr("Memory"));
    memory->setCheckable(true);
    memory->setChecked(CFG->PerfShowMemory);

    QAction *swap = menu.addAction(tr("Swap"));
    swap->setCheckable(true);
    swap->setChecked(CFG->PerfShowSwap);

    QAction *disks = menu.addAction(tr("Disks"));
    disks->setCheckable(true);
    disks->setChecked(CFG->PerfShowDisks);

    QAction *network = menu.addAction(tr("NICs"));
    network->setCheckable(true);
    network->setChecked(CFG->PerfShowNetwork);

    QAction *gpu = menu.addAction(tr("GPUs"));
    gpu->setCheckable(true);
    gpu->setChecked(CFG->PerfShowGpu);

    menu.addSeparator();
    QAction *customizeColors = menu.addAction(tr("Customize colors..."));

    menu.addSeparator();
    QMenu *timeMenu = menu.addMenu(tr("Graph time"));
    QVector<int> intervals = CFG->DataWindowAvailableIntervals;
    if (intervals.isEmpty())
        intervals.append(CFG->PerfGraphWindowSec);

    for (int sec : intervals)
    {
        QAction *a = timeMenu->addAction(Misc::SimplifyTime(sec));
        a->setCheckable(true);
        a->setChecked(CFG->PerfGraphWindowSec == sec);
        graphWindowActions.insert(a, sec);
    }

    QMenu *refreshMenu = menu.addMenu(tr("Refresh interval"));
    UIHelper::PopulateRefreshIntervalMenu(refreshMenu, refreshIntervalActions, pausedRefreshAction);

    QAction *picked = menu.exec(globalPos);
    if (!picked)
        return;

    if (graphWindowActions.contains(picked))
    {
        const int requestedWindow = graphWindowActions.value(picked);
        CFG->PerfGraphWindowSec = requestedWindow;
        this->applyGraphWindowSeconds();
        if (this->m_active)
            this->onProviderUpdated();
        return;
    }

    if (UIHelper::ApplyRefreshIntervalAction(picked, refreshIntervalActions, pausedRefreshAction, this->m_provider))
        return;

    if (picked == customizeColors)
    {
        ColorSchemeDialog dialog(this);
        if (dialog.exec() != QDialog::Accepted)
            return;

        CFG->UseCustomColorScheme = dialog.UseCustomScheme();
        const ColorScheme scheme = dialog.BuildScheme();
        CFG->CustomColorScheme = scheme.ToVariantMap();

        if (CFG->UseCustomColorScheme)
            ColorScheme::Install(new ColorScheme(scheme));
        else
            ColorScheme::Install(new ColorScheme(ColorScheme::DetectDarkMode()
                                                 ? ColorScheme::DefaultDark()
                                                 : ColorScheme::DefaultLight()));

        this->ApplyColorScheme();
        CFG->Save();
        return;
    }

    bool showCpu = CFG->PerfShowCpu;
    bool showMemory = CFG->PerfShowMemory;
    bool showSwap = CFG->PerfShowSwap;
    bool showDisks = CFG->PerfShowDisks;
    bool showNetwork = CFG->PerfShowNetwork;
    bool showGpu = CFG->PerfShowGpu;

    if (picked == cpu)
        showCpu = cpu->isChecked();
    else if (picked == memory)
        showMemory = memory->isChecked();
    else if (picked == swap)
        showSwap = swap->isChecked();
    else if (picked == disks)
        showDisks = disks->isChecked();
    else if (picked == network)
        showNetwork = network->isChecked();
    else if (picked == gpu)
        showGpu = gpu->isChecked();

    if (!(showCpu || showMemory || showSwap || showDisks || showNetwork || showGpu))
        return;

    CFG->PerfShowCpu = showCpu;
    CFG->PerfShowMemory = showMemory;
    CFG->PerfShowSwap = showSwap;
    CFG->PerfShowDisks = showDisks;
    CFG->PerfShowNetwork = showNetwork;
    CFG->PerfShowGpu = showGpu;

    this->applyPanelVisibility();
    this->updateSamplingPolicy();
    if (this->m_active)
        this->onProviderUpdated();
}

void PerformanceWidget::ApplyColorScheme()
{
    const ColorScheme *scheme = ColorScheme::GetCurrent();
    this->m_sidePanel->ApplyColorScheme();

    auto applySidePanelItem = [](Perf::SidePanelItem *item, const QColor &line, const QColor &fill)
    {
        if (!item)
            return;
        item->SetGraphColor(line, fill);
        item->update();
    };

    applySidePanelItem(this->m_sidePanel->GetItemAt(this->m_cpuPanelIndex), scheme->CpuGraphLineColor, scheme->CpuGraphFillColor);
    applySidePanelItem(this->m_sidePanel->GetItemAt(this->m_memoryPanelIndex), scheme->MemoryGraphLineColor, scheme->MemoryGraphFillColor);
    applySidePanelItem(this->m_sidePanel->GetItemAt(this->m_swapPanelIndex), scheme->SwapUsageGraphLineColor, scheme->SwapUsageGraphFillColor);

    for (Perf::SidePanelItem *item : std::as_const(this->m_diskItems))
        applySidePanelItem(item, scheme->DiskGraphLineColor, scheme->DiskGraphFillColor);
    for (Perf::SidePanelItem *item : std::as_const(this->m_networkItems))
        applySidePanelItem(item, scheme->NetworkGraphLineColor, scheme->NetworkGraphFillColor);
    for (Perf::SidePanelItem *item : std::as_const(this->m_gpuItems))
        applySidePanelItem(item, scheme->GpuGraphLineColor, scheme->GpuGraphFillColor);

    this->m_cpuDetail->ApplyColorScheme();
    this->m_memDetail->ApplyColorScheme();
    this->m_swapDetail->ApplyColorScheme();
    for (Perf::DiskDetailWidget *detail : std::as_const(this->m_diskDetails))
        if (detail)
            detail->ApplyColorScheme();
    for (Perf::NetworkDetailWidget *detail : std::as_const(this->m_networkDetails))
        if (detail)
            detail->ApplyColorScheme();
    for (Perf::GpuDetailWidget *detail : std::as_const(this->m_gpuDetails))
        if (detail)
            detail->ApplyColorScheme();

    this->m_sidePanel->update();
    this->update();
}

void PerformanceWidget::applyPanelVisibility()
{
    if (!(CFG->PerfShowCpu || CFG->PerfShowMemory || CFG->PerfShowSwap || CFG->PerfShowDisks || CFG->PerfShowNetwork || CFG->PerfShowGpu))
        CFG->PerfShowCpu = true;

    this->m_sidePanel->SetItemVisible(this->m_cpuPanelIndex, CFG->PerfShowCpu);
    this->m_sidePanel->SetItemVisible(this->m_memoryPanelIndex, CFG->PerfShowMemory);
    this->m_sidePanel->SetItemVisible(this->m_swapPanelIndex, CFG->PerfShowSwap);

    for (int i = 0; i < this->m_diskItems.size(); ++i)
        this->m_sidePanel->SetItemVisible(this->m_diskPanelStart + i, CFG->PerfShowDisks);
    for (int i = 0; i < this->m_networkItems.size(); ++i)
        this->m_sidePanel->SetItemVisible(this->m_networkPanelStart + i, CFG->PerfShowNetwork);
    for (int i = 0; i < this->m_gpuItems.size(); ++i)
        this->m_sidePanel->SetItemVisible(this->m_gpuPanelStart + i, CFG->PerfShowGpu);

    const int first = this->m_sidePanel->FirstVisibleIndex();
    if (first >= 0 && !this->m_sidePanel->IsItemVisible(this->m_sidePanel->GetCurrentIndex()))
        this->m_sidePanel->SetCurrentIndex(first);
}

void PerformanceWidget::updateSamplingPolicy()
{
    this->m_provider->SetCpuSamplingEnabled(CFG->PerfShowCpu);
    this->m_provider->SetMemorySamplingEnabled(CFG->PerfShowMemory || CFG->PerfShowSwap);
    this->m_provider->SetSwapSamplingEnabled(CFG->PerfShowSwap);
    this->m_provider->SetDiskSamplingEnabled(CFG->PerfShowDisks);
    this->m_provider->SetNetworkSamplingEnabled(CFG->PerfShowNetwork);
    this->m_provider->SetGpuSamplingEnabled(CFG->PerfShowGpu);
    this->m_provider->SetProcessStatsEnabled(CFG->PerfShowCpu && this->m_sidePanel->GetCurrentIndex() == this->m_cpuPanelIndex);
}

void PerformanceWidget::tagTimeAxisLabels()
{
    static const QRegularExpression kSecondsRe("^[0-9]+\\s+seconds$");
    for (QLabel *label : this->findChildren<QLabel *>())
    {
        if (!label)
            continue;
        if (kSecondsRe.match(label->text()).hasMatch())
            label->setProperty("perfTimeAxisLabel", true);
    }
}

void PerformanceWidget::applyGraphWindowSeconds()
{
    const int sec = CFG->PerfGraphWindowSec;
    for (Perf::GraphWidget *g : this->findChildren<Perf::GraphWidget *>())
    {
        if (g)
            g->SetSampleCapacity(sec);
    }

    const QString labelText = Misc::SimplifyTime(sec);

    for (QLabel *label : this->findChildren<QLabel *>())
    {
        if (label && label->property("perfTimeAxisLabel").toBool())
            label->setText(labelText);
    }
}
