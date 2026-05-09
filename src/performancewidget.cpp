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
#include "metrics.h"
#include "ui_performancewidget.h"

#include "colorschemedialog.h"
#include "configuration.h"
#include "colorscheme.h"
#include "logger.h"
#include "misc.h"
#include "perf/graphwidget.h"
#include "perf/sidepanelgroup.h"
#include "perf/sidepanelorderdialog.h"
#include "ui/uihelper.h"

#include <QAction>
#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPalette>
#include <QRegularExpression>

namespace
{
    QList<Perf::SidePanelGroup> sanitizeSidePanelGroupOrder(const QStringList &storedOrder)
    {
        const QList<Perf::SidePanelGroup> defaults = Perf::DefaultSidePanelGroupOrder();
        QList<Perf::SidePanelGroup> sanitized;
        for (const QString &id : storedOrder)
        {
            const auto group = Perf::SidePanelGroupFromId(id);
            if (group.has_value() && !sanitized.contains(*group))
                sanitized.append(*group);
        }

        for (Perf::SidePanelGroup group : defaults)
        {
            if (!sanitized.contains(group))
                sanitized.append(group);
        }

        return sanitized;
    }

    QStringList serializeSidePanelGroupOrder(const QList<Perf::SidePanelGroup> &order)
    {
        QStringList out;
        out.reserve(order.size());
        for (Perf::SidePanelGroup group : order)
            out.append(Perf::SidePanelGroupId(group));
        return out;
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Construction
////////////////////////////////////////////////////////////////////////////////////////////////////

PerformanceWidget *PerformanceWidget::s_instance = nullptr;

PerformanceWidget::PerformanceWidget(QWidget *parent) : QWidget(parent), ui(new Ui::PerformanceWidget)
{
    s_instance = this;

    // Ensure the metrics are initialized before we start enumerating devices
    Metrics::Get();

    this->m_sidePanel = new Perf::SidePanel(this);
    this->m_stack = new QStackedWidget(this);
    this->m_cpuDetail = new Perf::CpuDetailWidget(this);
    this->m_memDetail = new Perf::MemoryDetailWidget(this);
    this->m_swapDetail = new Perf::SwapDetailWidget(this);

    this->ui->setupUi(this);

    this->setupLayout();
    this->setupSidePanel();
    this->applySidePanelOrder();

    // Wire detail widgets to the data provider
    this->m_cpuDetail->Init();
    this->m_memDetail->Init();
    this->m_swapDetail->Init();

    // Update side panel thumbnails on every sample
    connect(Metrics::Get(), &Metrics::updated, this, &PerformanceWidget::onProviderUpdated);

    // Expensive process/thread counting is only needed for CPU detail page.
    connect(this->m_sidePanel, &Perf::SidePanel::currentChanged, this, [this](Perf::SidePanelItem *item)
    {
        QWidget *detail = this->m_detailByItem.value(item, nullptr);
        if (detail)
            this->m_stack->setCurrentWidget(detail);
        Metrics::Get()->SetProcessStatsEnabled(item == this->m_cpuItem && CFG->PerfShowCpu);
    });
    connect(this->m_sidePanel, &Perf::SidePanel::itemContextMenuRequested, this, &PerformanceWidget::onSidePanelContextMenu);

    this->tagTimeAxisLabels();
    this->applyGraphWindowSeconds();
    this->applyPanelVisibility();
    this->updateSamplingPolicy();
    Metrics::Get()->SetProcessStatsEnabled(this->m_sidePanel->GetCurrentItem() == this->m_cpuItem && CFG->PerfShowCpu);

    this->SetActive(false);

    LOG_DEBUG("PerformanceWidget initialised");
}

PerformanceWidget::~PerformanceWidget()
{
    s_instance = nullptr;
    delete this->ui;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Private setup
////////////////////////////////////////////////////////////////////////////////////////////////////

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

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    // CPU item
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    this->m_cpuItem = new Perf::SidePanelItem(tr("CPU"), this);
    this->m_cpuItem->SetGraphColor(scheme->CpuGraphLineColor, scheme->CpuGraphFillColor);
    this->m_cpuItem->SetGraphSource(Metrics::GetCPU()->CpuHistory());
    this->m_sidePanel->AddItem(this->m_cpuItem);
    this->m_stack->addWidget(this->m_cpuDetail);
    this->m_detailByItem.insert(this->m_cpuItem, this->m_cpuDetail);

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    // Memory item
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    this->m_memoryItem = new Perf::SidePanelItem(tr("Memory"), this);
    this->m_memoryItem->SetGraphColor(scheme->MemoryGraphLineColor, scheme->MemoryGraphFillColor);
    this->m_memoryItem->SetGraphSource(Metrics::GetMemory()->MemHistory());
    this->m_sidePanel->AddItem(this->m_memoryItem);
    this->m_stack->addWidget(this->m_memDetail);
    this->m_detailByItem.insert(this->m_memoryItem, this->m_memDetail);

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    // Swap item
    ////////////////////////////////////////////////////////////////////////////////////////////////////
    this->m_swapItem = new Perf::SidePanelItem(tr("Swap"), this);
    this->m_swapItem->SetGraphColor(scheme->SwapUsageGraphLineColor, scheme->SwapUsageGraphFillColor);
    this->m_swapItem->SetGraphSource(Metrics::GetSwap()->SwapUsageHistory());
    this->m_sidePanel->AddItem(this->m_swapItem);
    this->m_stack->addWidget(this->m_swapDetail);
    this->m_detailByItem.insert(this->m_swapItem, this->m_swapDetail);

    this->setupDiskPanels();
    this->setupNetworkPanels();
    this->setupGpuPanels();
}

void PerformanceWidget::setupDiskPanels()
{
    const ColorScheme *scheme = ColorScheme::GetCurrent();
    const int count = Metrics::GetStorage()->DiskCount();
    for (int i = 0; i < count; ++i)
    {
        const Storage::DiskInfo &disk = Metrics::GetStorage()->FromIndex(i);
        this->m_diskNames.append(disk.Name);

        auto *item = new Perf::SidePanelItem(tr("Disk (%1)").arg(disk.Name), this);
        item->SetGraphColor(scheme->DiskGraphLineColor, scheme->DiskGraphFillColor);
        item->SetGraphSource(disk.ActiveHistory);
        this->m_sidePanel->AddItem(item);
        this->m_diskItems.append(item);

        auto *detail = new Perf::DiskDetailWidget(this);
        detail->SetDisk(i);
        this->m_stack->addWidget(detail);
        this->m_diskDetails.append(detail);
        this->m_detailByItem.insert(item, detail);
    }
}

void PerformanceWidget::setupGpuPanels()
{
    const ColorScheme *scheme = ColorScheme::GetCurrent();
    const int count = Metrics::GetGPU()->GpuCount();
    for (int i = 0; i < count; ++i)
    {
        const GPU::GPUInfo &gpu = Metrics::GetGPU()->FromIndex(i);
        this->m_gpuNames.append(gpu.Name);

        auto *item = new Perf::SidePanelItem(tr("GPU %1").arg(i), this);
        item->SetGraphColor(scheme->GpuGraphLineColor, scheme->GpuGraphFillColor);
        item->SetGraphSource(gpu.UtilHistory);
        this->m_sidePanel->AddItem(item);
        this->m_gpuItems.append(item);

        auto *detail = new Perf::GpuDetailWidget(this);
        detail->SetGpu(i);
        this->m_stack->addWidget(detail);
        this->m_gpuDetails.append(detail);
        this->m_detailByItem.insert(item, detail);
    }
}

void PerformanceWidget::setupNetworkPanels()
{
    const ColorScheme *scheme = ColorScheme::GetCurrent();
    const int count = Metrics::GetNetwork()->NetworkCount();
    for (int i = 0; i < count; ++i)
    {
        const Network::NetworkInfo &network = Metrics::GetNetwork()->FromIndex(i);
        this->m_networkNames.append(network.Name);

        auto *item = new Perf::SidePanelItem(tr("NIC (%1)").arg(network.Name), this);
        item->SetGraphColor(scheme->NetworkGraphLineColor, scheme->NetworkGraphFillColor);
        item->SetGraphSource(network.RxHistory, 1024.0);
        this->m_sidePanel->AddItem(item);
        this->m_networkItems.append(item);

        auto *detail = new Perf::NetworkDetailWidget(this);
        detail->SetNetwork(i);
        this->m_stack->addWidget(detail);
        this->m_networkDetails.append(detail);
        this->m_detailByItem.insert(item, detail);
    }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// Slots
////////////////////////////////////////////////////////////////////////////////////////////////////

void PerformanceWidget::onProviderUpdated()
{
    // Update CPU side panel item
    const double cpuPct = Metrics::GetCPU()->CpuPercent();
    const int cpuTempC = Metrics::GetCPU()->CpuTemperatureC();
    const QString cpuSub = (cpuTempC >= 0)
                           ? tr("%1%2 %3C", "%1=value %2=percent sign %3=temperature in Celsius")
                                 .arg(QString::number(cpuPct, 'f', 0), "%", QString::number(cpuTempC))
                           : QString::number(cpuPct, 'f', 0) + "%";
    if (CFG->PerfShowCpu)
        this->m_cpuItem->Update(cpuSub);

    // Update Memory side panel item
    const qint64 used  = Metrics::GetMemory()->MemUsedKb();
    const qint64 total = Metrics::GetMemory()->MemTotalKb();
    const int    pct     = total > 0
                           ? static_cast<int>(static_cast<double>(used) / total * 100.0)
                           : 0;
    const QString memSub = QString("%1/%2 (%3%)")
                           .arg(Misc::FormatKiB(static_cast<quint64>(qMax<qint64>(0, used)), 1),
                                Misc::FormatKiB(static_cast<quint64>(qMax<qint64>(0, total)), 1),
                                QString::number(pct));
    if (CFG->PerfShowMemory)
        this->m_memoryItem->Update(memSub);

    // Update Swap side panel item
    const qint64 swapUsed = Metrics::GetSwap()->SwapUsedKb();
    const qint64 swapTotal = Metrics::GetSwap()->SwapTotalKb();
    const int swapPct = (swapTotal > 0)
                        ? static_cast<int>(static_cast<double>(swapUsed) / static_cast<double>(swapTotal) * 100.0)
                        : 0;
    QString swapSub;
    if (swapTotal > 0)
    {
        swapSub = QString("%1/%2 (%3%)")
                  .arg(Misc::FormatKiB(static_cast<quint64>(qMax<qint64>(0, swapUsed)), 1),
                       Misc::FormatKiB(static_cast<quint64>(qMax<qint64>(0, swapTotal)), 1),
                       QString::number(swapPct));
    } else
    {
        swapSub = tr("Off");
    }

    if (CFG->PerfShowSwap)
        this->m_swapItem->Update(swapSub);

    if (CFG->PerfShowDisks)
    {
        for (int i = 0; i < this->m_diskItems.size(); ++i)
        {
            if (i >= Metrics::GetStorage()->DiskCount())
                break;
            auto *item = this->m_diskItems.at(i);
            if (!item)
                continue;

            const Storage::DiskInfo &disk = Metrics::GetStorage()->FromIndex(i);
            const QString diskSub = tr("%1 %2", "%1=disk type %2=active percentage").arg(disk.Type, QString::number(disk.ActivePct, 'f', 0) + "%");
            item->Update(diskSub);
        }
    }

    if (CFG->PerfShowGpu)
    {
        for (int i = 0; i < this->m_gpuItems.size(); ++i)
        {
            if (i >= Metrics::GetGPU()->GpuCount())
                break;
            auto *item = this->m_gpuItems.at(i);
            if (!item)
                continue;

            const GPU::GPUInfo &gpu = Metrics::GetGPU()->FromIndex(i);
            const QString utilText = tr("%1%2", "%1=GPU utilization value %2=percent sign").arg(QString::number(gpu.UtilPct, 'f', 0), "%");
            const int tempC = gpu.TemperatureC;
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
            if (i >= Metrics::GetNetwork()->NetworkCount())
                break;
            auto *item = this->m_networkItems.at(i);
            if (!item)
                continue;

            const Network::NetworkInfo &network = Metrics::GetNetwork()->FromIndex(i);
            const QString uploadRate = CFG->PerfNetworkUseBits
                                       ? Misc::FormatBitsPerSecond(network.TxBps)
                                       : Misc::FormatBytesPerSecond(network.TxBps);
            const QString downloadRate = CFG->PerfNetworkUseBits
                                         ? Misc::FormatBitsPerSecond(network.RxBps)
                                         : Misc::FormatBytesPerSecond(network.RxBps);
            const QString netSub = tr("U:%1 D:%2", "%1=upload rate %2=download rate").arg(uploadRate, downloadRate);
            item->Update(netSub, network.MaxThroughputBps);
        }
    }
}

void PerformanceWidget::SetActive(bool active)
{
    if (this->m_active == active)
        return;

    this->m_active = active;
    Metrics::Get()->SetActive(active);
    if (active)
        this->onProviderUpdated();
}

void PerformanceWidget::onSidePanelContextMenu(Perf::SidePanelItem * /*item*/, const QPoint &globalPos)
{
    QMenu menu(this);

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
    QMenu *settingsMenu = menu.addMenu(tr("Settings"));
    QAction *customizeOrder = settingsMenu->addAction(tr("Customize order..."));
    QAction *customizeColors = settingsMenu->addAction(tr("Customize colors..."));
    QAction *showGrid = settingsMenu->addAction(tr("Show grid in side panel"));
    showGrid->setCheckable(true);
    showGrid->setChecked(CFG->SidePanelGridEnabled);

    menu.addSeparator();
    UIHelper::AddRefreshIntervalContextMenu(&menu, nullptr, this->m_active);
    UIHelper::AddGraphWindowContextMenu(&menu);

    menu.addSeparator();
    UIHelper::AddGlobalContextMenuItems(&menu, this);

    QAction *picked = menu.exec(globalPos);
    if (!picked)
        return;

    if (picked == customizeOrder)
    {
        SidePanelOrderDialog dialog(sanitizeSidePanelGroupOrder(CFG->PerfSidePanelGroupOrder), this);
        if (dialog.exec() != QDialog::Accepted)
            return;

        CFG->PerfSidePanelGroupOrder = serializeSidePanelGroupOrder(dialog.GetOrder());
        this->applySidePanelOrder();
        CFG->Save();
        return;
    }

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

    if (picked == showGrid)
    {
        CFG->SidePanelGridEnabled = showGrid->isChecked();
        this->applySidePanelGridEnabled();
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

    applySidePanelItem(this->m_cpuItem, scheme->CpuGraphLineColor, scheme->CpuGraphFillColor);
    applySidePanelItem(this->m_memoryItem, scheme->MemoryGraphLineColor, scheme->MemoryGraphFillColor);
    applySidePanelItem(this->m_swapItem, scheme->SwapUsageGraphLineColor, scheme->SwapUsageGraphFillColor);

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

void PerformanceWidget::applySidePanelOrder()
{
    const QList<Perf::SidePanelGroup> order = sanitizeSidePanelGroupOrder(CFG->PerfSidePanelGroupOrder);
    CFG->PerfSidePanelGroupOrder = serializeSidePanelGroupOrder(order);

    QList<Perf::SidePanelItem *> items;
    items.reserve(this->m_sidePanel->GetCount());

    for (Perf::SidePanelGroup group : order)
    {
        switch (group)
        {
            case Perf::SidePanelGroup::Cpu:
                items.append(this->m_cpuItem);
                break;
            case Perf::SidePanelGroup::Memory:
                items.append(this->m_memoryItem);
                break;
            case Perf::SidePanelGroup::Swap:
                items.append(this->m_swapItem);
                break;
            case Perf::SidePanelGroup::Disks:
                for (Perf::SidePanelItem *item : std::as_const(this->m_diskItems))
                    items.append(item);
                break;
            case Perf::SidePanelGroup::Network:
                for (Perf::SidePanelItem *item : std::as_const(this->m_networkItems))
                    items.append(item);
                break;
            case Perf::SidePanelGroup::Gpu:
                for (Perf::SidePanelItem *item : std::as_const(this->m_gpuItems))
                    items.append(item);
                break;
        }
    }

    this->m_sidePanel->SetItemOrder(items);
}

void PerformanceWidget::applyPanelVisibility()
{
    if (!(CFG->PerfShowCpu || CFG->PerfShowMemory || CFG->PerfShowSwap || CFG->PerfShowDisks || CFG->PerfShowNetwork || CFG->PerfShowGpu))
        CFG->PerfShowCpu = true;

    this->m_sidePanel->SetItemVisible(this->m_cpuItem, CFG->PerfShowCpu);
    this->m_sidePanel->SetItemVisible(this->m_memoryItem, CFG->PerfShowMemory);
    this->m_sidePanel->SetItemVisible(this->m_swapItem, CFG->PerfShowSwap);

    for (Perf::SidePanelItem *item : std::as_const(this->m_diskItems))
        this->m_sidePanel->SetItemVisible(item, CFG->PerfShowDisks);
    for (Perf::SidePanelItem *item : std::as_const(this->m_networkItems))
        this->m_sidePanel->SetItemVisible(item, CFG->PerfShowNetwork);
    for (Perf::SidePanelItem *item : std::as_const(this->m_gpuItems))
        this->m_sidePanel->SetItemVisible(item, CFG->PerfShowGpu);

    Perf::SidePanelItem *first = this->m_sidePanel->FirstVisibleItem();
    if (first && !this->m_sidePanel->IsItemVisible(this->m_sidePanel->GetCurrentItem()))
        this->m_sidePanel->SetCurrentItem(first);
}

void PerformanceWidget::updateSamplingPolicy()
{
    Metrics::Get()->SetCpuSamplingEnabled(CFG->PerfShowCpu);
    Metrics::Get()->SetMemorySamplingEnabled(CFG->PerfShowMemory);
    Metrics::Get()->SetSwapSamplingEnabled(CFG->PerfShowSwap);
    Metrics::Get()->SetDiskSamplingEnabled(CFG->PerfShowDisks);
    Metrics::Get()->SetNetworkSamplingEnabled(CFG->PerfShowNetwork);
    Metrics::Get()->SetGpuSamplingEnabled(CFG->PerfShowGpu);
    Metrics::Get()->SetProcessStatsEnabled(CFG->PerfShowCpu && this->m_sidePanel->GetCurrentItem() == this->m_cpuItem);
}

void PerformanceWidget::applySidePanelGridEnabled()
{
    const bool enabled = CFG->SidePanelGridEnabled;

    if (this->m_cpuItem)
        this->m_cpuItem->SetGraphGridEnabled(enabled);
    if (this->m_memoryItem)
        this->m_memoryItem->SetGraphGridEnabled(enabled);
    if (this->m_swapItem)
        this->m_swapItem->SetGraphGridEnabled(enabled);

    for (Perf::SidePanelItem *item : std::as_const(this->m_diskItems))
        item->SetGraphGridEnabled(enabled);
    for (Perf::SidePanelItem *item : std::as_const(this->m_networkItems))
        item->SetGraphGridEnabled(enabled);
    for (Perf::SidePanelItem *item : std::as_const(this->m_gpuItems))
        item->SetGraphGridEnabled(enabled);
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
