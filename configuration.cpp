#include "configuration.h"

#include <QSettings>
#include <QVariantList>

Configuration *Configuration::s_instance = nullptr;

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
    QSettings s;

    // Window
    this->WindowGeometry = s.value("Window/Geometry").toByteArray();
    this->WindowState    = s.value("Window/State").toByteArray();
    this->ActiveTab      = s.value("Window/ActiveTab", this->ActiveTab).toInt();

    // General
    this->RefreshRateMs  = s.value("General/RefreshRateMs", this->RefreshRateMs).toInt();

    // Processes
    this->ShowKernelTasks        = s.value("Processes/ShowKernelTasks",        this->ShowKernelTasks).toBool();
    this->ShowOtherUsersProcs    = s.value("Processes/ShowOtherUsersProcs",    this->ShowOtherUsersProcs).toBool();
    this->ProcessListSortColumn  = s.value("Processes/SortColumn",             this->ProcessListSortColumn).toInt();
    this->ProcessListSortOrder   = s.value("Processes/SortOrder",              this->ProcessListSortOrder).toInt();

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
    this->CpuShowKernelTimes = s.value("Performance/CpuShowKernelTimes",
                                       this->CpuShowKernelTimes).toBool();
}

void Configuration::Save()
{
    QSettings s;

    // Window
    s.setValue("Window/Geometry",  this->WindowGeometry);
    s.setValue("Window/State",     this->WindowState);
    s.setValue("Window/ActiveTab", this->ActiveTab);

    // General
    s.setValue("General/RefreshRateMs", this->RefreshRateMs);

    // Processes
    s.setValue("Processes/ShowKernelTasks",     this->ShowKernelTasks);
    s.setValue("Processes/ShowOtherUsersProcs", this->ShowOtherUsersProcs);
    s.setValue("Processes/SortColumn",          this->ProcessListSortColumn);
    s.setValue("Processes/SortOrder",           this->ProcessListSortOrder);

    QVariantList gpuSel;
    gpuSel.reserve(this->GpuEngineSelectorIndices.size());
    for (int v : this->GpuEngineSelectorIndices)
        gpuSel.append(v);
    s.setValue("Performance/GpuEngineSelectorIndices", gpuSel);
    s.setValue("Performance/CpuGraphMode", this->CpuGraphMode);
    s.setValue("Performance/CpuShowKernelTimes", this->CpuShowKernelTimes);

    s.sync();
}
