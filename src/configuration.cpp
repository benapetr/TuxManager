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

#include "configuration.h"
#include "colorscheme.h"
#include "globals.h"

#include <QApplication>
#include <QPalette>
#include <QSettings>
#include <QVariantList>

#include <unistd.h>

Configuration *Configuration::s_instance = nullptr;

namespace
{
    // Temporary migration fallback for the legacy "Tux Manager" settings path
    // this is only for loading of settings - we never write to old path
    std::unique_ptr<QSettings> getSettings()
    {
        auto current = std::make_unique<QSettings>();

        // If the new path is already containing data use it to read settings, otherwise try fallback to old path
        if (!current->allKeys().isEmpty())
            return current;

        auto legacy = std::make_unique<QSettings>(
            QSettings::NativeFormat,
            QSettings::UserScope,
            "Tux Manager",
            "Tux Manager");

        if (!legacy->allKeys().isEmpty())
            return legacy;

        // Neither new or old path contains any data, use new path
        return current;
    }
}

Configuration *Configuration::instance()
{
    if (!s_instance)
        s_instance = new Configuration();
    return s_instance;
}

Configuration::Configuration(QObject *parent) : QObject(parent)
{}

void Configuration::Load()
{
    // QSettings s;
    // Let's keep this for like 5 more versions then we can restore original simple loading mechanism
    std::unique_ptr<QSettings> temp = getSettings();
    QSettings &s = *temp;

    // Window
    this->WindowGeometry = s.value("Window/Geometry").toByteArray();
    this->WindowState    = s.value("Window/State").toByteArray();
    this->ActiveTab      = s.value("Window/ActiveTab", this->ActiveTab).toInt();

    // General
    this->RefreshRateMs =        s.value("General/RefreshRateMs",        this->RefreshRateMs).toInt();
    this->RefreshPaused =        s.value("General/RefreshPaused",        this->RefreshPaused).toBool();
    this->UseCustomColorScheme = s.value("General/UseCustomColorScheme", this->UseCustomColorScheme).toBool();
    this->CustomColorScheme =    s.value("General/CustomColorScheme",    this->CustomColorScheme).toMap();
    this->EUID = ::geteuid();
    this->IsSuperuser = (this->EUID == 0);

    // Processes
    this->ShowKernelTasks        = s.value("Processes/ShowKernelTasks",     this->ShowKernelTasks).toBool();
    this->ShowOtherUsersProcs    = s.value("Processes/ShowOtherUsersProcs", this->ShowOtherUsersProcs).toBool();
    this->ProcessTreeView        = s.value("Processes/TreeView",            this->ProcessTreeView).toBool();
    this->ProcessListSortColumn  = s.value("Processes/SortColumn",          this->ProcessListSortColumn).toInt();
    this->ProcessListSortOrder   = s.value("Processes/SortOrder",           this->ProcessListSortOrder).toInt();
    this->ProcessListHeaderState = s.value("Processes/TableHeaderState",    this->ProcessListHeaderState).toByteArray();
    this->ProcessTreeHeaderState = s.value("Processes/TreeHeaderState",     this->ProcessTreeHeaderState).toByteArray();
    this->TaskHistory            = s.value("Processes/TaskHistory",         this->TaskHistory).toStringList();
    this->LastTaskDirectory      = s.value("Processes/LastTaskDirectory",   this->LastTaskDirectory).toString();
    while (this->TaskHistory.size() > TUX_MANAGER_TASK_HISTORY)
        this->TaskHistory.removeLast();

    // Services
    this->ServicesHeaderState    = s.value("Services/HeaderState",          this->ServicesHeaderState).toByteArray();

    // Performance / GPU selectors
    const QVariantList gpuSel = s.value("Performance/GpuEngineSelectorIndices").toList();
    if (!gpuSel.isEmpty())
    {
        this->GpuEngineSelectorIndices.clear();
        this->GpuEngineSelectorIndices.reserve(gpuSel.size());
        for (const QVariant &v : gpuSel)
            this->GpuEngineSelectorIndices.append(v.toInt());
    }

    while (this->GpuEngineSelectorIndices.size() < 4)
        this->GpuEngineSelectorIndices.append(this->GpuEngineSelectorIndices.size());
    if (this->GpuEngineSelectorIndices.size() > 4)
        this->GpuEngineSelectorIndices.resize(4);
    this->CpuGraphMode = s.value("Performance/CpuGraphMode", this->CpuGraphMode).toInt();
    if (this->CpuGraphMode != 0 && this->CpuGraphMode != 1)
        this->CpuGraphMode = 0;
    this->SwapGraphMode = s.value("Performance/SwapGraphMode", this->SwapGraphMode).toInt();
    if (this->SwapGraphMode != 0 && this->SwapGraphMode != 1)
        this->SwapGraphMode = 0;
    this->CpuShowKernelTimes = s.value("Performance/CpuShowKernelTimes", this->CpuShowKernelTimes).toBool();
    this->PerfShowCpu =         s.value("Performance/ShowCpu",           this->PerfShowCpu).toBool();
    this->PerfShowMemory =      s.value("Performance/ShowMemory",        this->PerfShowMemory).toBool();
    this->PerfShowSwap =        s.value("Performance/ShowSwap",          this->PerfShowSwap).toBool();
    this->PerfShowDisks =       s.value("Performance/ShowDisks",         this->PerfShowDisks).toBool();
    this->PerfShowNetwork =     s.value("Performance/ShowNetwork",       this->PerfShowNetwork).toBool();
    this->PerfShowGpu =         s.value("Performance/ShowGpu",           this->PerfShowGpu).toBool();
    this->SidePanelGridEnabled = s.value("Performance/SidePanelGridEnabled", this->SidePanelGridEnabled).toBool();
    this->PerfNetworkUseBits =  s.value("Performance/NetworkUseBits",    this->PerfNetworkUseBits).toBool();
    this->PerfGraphWindowSec =  s.value("Performance/GraphWindowSec",    this->PerfGraphWindowSec).toInt();
    this->PerfSidePanelGroupOrder = s.value("Performance/SidePanelGroupOrder", this->PerfSidePanelGroupOrder).toStringList();
    this->PerformanceSplitterState = s.value("Performance/SplitterState",     this->PerformanceSplitterState).toByteArray();

    // For now this is hardcoded, we may want to make it customizable later
    this->RefreshRateAvailableIntervals = { 250, 500, 1000, 2000, 5000, 15000 };
    this->DataWindowAvailableIntervals = { 60, 120, 300, 900 };

    if (!this->RefreshRateAvailableIntervals.contains(this->RefreshRateMs))
        this->RefreshRateMs = this->RefreshRateAvailableIntervals[0];

    if (!this->DataWindowAvailableIntervals.contains(this->PerfGraphWindowSec))
        this->PerfGraphWindowSec = this->DataWindowAvailableIntervals[0];

    // Color scheme
    // we always start with either dark or light default even if there is customization layered over it,
    // this is for future version compatibility, so that if any color is added, we always load default
    // first and then we overwrite it with customizations (missing custom color won't break stuff)
    ColorScheme *scheme = new ColorScheme(ColorScheme::DetectDarkMode()
                                          ? ColorScheme::DefaultDark()
                                          : ColorScheme::DefaultLight());
    if (this->UseCustomColorScheme)
        scheme->ApplyVariantMap(this->CustomColorScheme);
    ColorScheme::Install(scheme);
}

void Configuration::Save()
{
    QSettings s;

    // Window
    s.setValue("Window/Geometry",               this->WindowGeometry);
    s.setValue("Window/State",                  this->WindowState);
    s.setValue("Window/ActiveTab",              this->ActiveTab);

    // General
    s.setValue("General/RefreshRateMs",         this->RefreshRateMs);
    s.setValue("General/RefreshPaused",         this->RefreshPaused);
    s.setValue("General/UseCustomColorScheme",  this->UseCustomColorScheme);
    s.setValue("General/CustomColorScheme",     this->CustomColorScheme);

    // Processes
    s.setValue("Processes/ShowKernelTasks",     this->ShowKernelTasks);
    s.setValue("Processes/ShowOtherUsersProcs", this->ShowOtherUsersProcs);
    s.setValue("Processes/TreeView",            this->ProcessTreeView);
    s.setValue("Processes/SortColumn",          this->ProcessListSortColumn);
    s.setValue("Processes/SortOrder",           this->ProcessListSortOrder);
    s.setValue("Processes/TableHeaderState",    this->ProcessListHeaderState);
    s.setValue("Processes/TreeHeaderState",     this->ProcessTreeHeaderState);
    s.setValue("Processes/TaskHistory",         this->TaskHistory);
    s.setValue("Processes/LastTaskDirectory",   this->LastTaskDirectory);

    // Services
    s.setValue("Services/HeaderState",          this->ServicesHeaderState);

    QVariantList gpuSel;
    gpuSel.reserve(this->GpuEngineSelectorIndices.size());
    for (int v : this->GpuEngineSelectorIndices)
        gpuSel.append(v);

    s.setValue("Performance/GpuEngineSelectorIndices",  gpuSel);
    s.setValue("Performance/CpuGraphMode",              this->CpuGraphMode);
    s.setValue("Performance/SwapGraphMode",             this->SwapGraphMode);
    s.setValue("Performance/CpuShowKernelTimes",        this->CpuShowKernelTimes);
    s.setValue("Performance/ShowCpu",                   this->PerfShowCpu);
    s.setValue("Performance/ShowMemory",                this->PerfShowMemory);
    s.setValue("Performance/ShowSwap",                  this->PerfShowSwap);
    s.setValue("Performance/ShowDisks",                 this->PerfShowDisks);
    s.setValue("Performance/ShowNetwork",               this->PerfShowNetwork);
    s.setValue("Performance/ShowGpu",                   this->PerfShowGpu);
    s.setValue("Performance/SidePanelGridEnabled",      this->SidePanelGridEnabled);
    s.setValue("Performance/NetworkUseBits",            this->PerfNetworkUseBits);
    s.setValue("Performance/GraphWindowSec",            this->PerfGraphWindowSec);
    s.setValue("Performance/SidePanelGroupOrder",       this->PerfSidePanelGroupOrder);
    s.setValue("Performance/SplitterState",             this->PerformanceSplitterState);

    s.sync();
}
