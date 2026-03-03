#include "gpudetailwidget.h"
#include "configuration.h"

#include <algorithm>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QPalette>
#include <QVBoxLayout>

namespace Perf
{

GpuDetailWidget::GpuDetailWidget(QWidget *parent)
    : QWidget(parent)
{
    this->m_selectedEngineBySlot = CFG->GpuEngineSelectorIndices;
    while (this->m_selectedEngineBySlot.size() < 4)
        this->m_selectedEngineBySlot.append(this->m_selectedEngineBySlot.size());
    if (this->m_selectedEngineBySlot.size() > 4)
        this->m_selectedEngineBySlot.resize(4);

    const QColor lineColor = this->palette().color(QPalette::Highlight);
    QColor fillColor = lineColor;
    fillColor.setAlpha(110);

    auto configureGraph = [&](GraphWidget *graph)
    {
        graph->setColor(lineColor, fillColor);
        graph->setSampleCapacity(HISTORY_SIZE);
        graph->setGridColumns(6);
        graph->setGridRows(4);
        graph->setValueFormat(GraphWidget::ValueFormat::Percent);
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
    this->m_dedicatedMemGraph->setSeriesNames(tr("Dedicated memory usage"));
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
    this->m_sharedMemGraph->setSeriesNames(tr("Shared memory usage"));
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
    this->m_copyBwLegendLabel->setStyleSheet("color:#888;");
    this->m_copyBwLegendLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    copyHeader->addWidget(this->m_copyBwLegendLabel, 1);
    this->m_copyBwGraphMaxLabel = new QLabel(tr("0 KB/s"), this);
    this->m_copyBwGraphMaxLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    copyHeader->addWidget(this->m_copyBwGraphMaxLabel);
    root->addLayout(copyHeader);

    this->m_copyBwGraph = new GraphWidget(this);
    configureGraph(this->m_copyBwGraph);
    this->m_copyBwGraph->setSeriesNames(tr("TX"), tr("RX"));
    this->m_copyBwGraph->setValueFormat(GraphWidget::ValueFormat::BytesPerSec);
    this->m_copyBwGraph->setMinimumHeight(70);
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
    stats->addWidget(mkLabel(tr("Driver version")), 2, 0);
    stats->addWidget(this->m_driverValueLabel, 2, 1);
    stats->addWidget(mkLabel(tr("Backend")), 2, 2);
    stats->addWidget(this->m_backendValueLabel, 2, 3);
    root->addLayout(stats);
}

void GpuDetailWidget::setProvider(PerfDataProvider *provider)
{
    if (this->m_provider)
        disconnect(this->m_provider, &PerfDataProvider::updated,
                   this, &GpuDetailWidget::onUpdated);

    this->m_provider = provider;

    if (this->m_provider)
    {
        connect(this->m_provider, &PerfDataProvider::updated,
                this, &GpuDetailWidget::onUpdated);
        this->onUpdated();
    }
}

void GpuDetailWidget::setGpuIndex(int index)
{
    this->m_gpuIndex = index;
    this->rebuildEngineSelectors();
    this->onUpdated();
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
    this->onUpdated();
}

void GpuDetailWidget::rebuildEngineSelectors()
{
    if (!this->m_provider || this->m_gpuIndex < 0 || this->m_gpuIndex >= this->m_provider->gpuCount())
        return;

    const int engineCount = this->m_provider->gpuEngineCount(this->m_gpuIndex);
    for (int slot = 0; slot < this->m_engineSelectors.size(); ++slot)
    {
        QComboBox *combo = this->m_engineSelectors.at(slot);
        if (!combo)
            continue;

        combo->blockSignals(true);
        combo->clear();

        for (int i = 0; i < engineCount; ++i)
            combo->addItem(this->m_provider->gpuEngineName(this->m_gpuIndex, i), i);

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
    if (!this->m_provider || this->m_gpuIndex < 0 || this->m_gpuIndex >= this->m_provider->gpuCount())
        return;

    this->m_titleLabel->setText(tr("GPU %1").arg(this->m_gpuIndex));
    this->m_modelLabel->setText(this->m_provider->gpuName(this->m_gpuIndex));

    const double util = this->m_provider->gpuUtilPercent(this->m_gpuIndex);
    const qint64 dedicatedUsedMiB = this->m_provider->gpuMemUsedMiB(this->m_gpuIndex);
    const qint64 dedicatedTotalMiB = this->m_provider->gpuMemTotalMiB(this->m_gpuIndex);

    // Best-effort approximation used by task-manager-like UIs on Linux where
    // shared GPU accounting is usually not exposed by common tooling.
    const qint64 sharedTotalMiB = qMax<qint64>(0, this->m_provider->memTotalKb() / 1024 / 2);
    const qint64 sharedUsedMiB = 0;

    this->m_sharedMemHistory.append(0.0);
    while (this->m_sharedMemHistory.size() > HISTORY_SIZE)
        this->m_sharedMemHistory.removeFirst();

    const qint64 gpuUsedMiB = dedicatedUsedMiB + sharedUsedMiB;
    const qint64 gpuTotalMiB = dedicatedTotalMiB + sharedTotalMiB;

    this->m_utilValueLabel->setText(QString::number(util, 'f', 0) + "%");
    this->m_gpuMemValueLabel->setText(tr("%1 / %2")
                                      .arg(formatMemMib(gpuUsedMiB))
                                      .arg(formatMemMib(gpuTotalMiB)));
    this->m_dedicatedMemValueLabel->setText(tr("%1 / %2")
                                            .arg(formatMemMib(dedicatedUsedMiB))
                                            .arg(formatMemMib(dedicatedTotalMiB)));
    this->m_sharedMemValueLabel->setText(tr("%1 / %2")
                                         .arg(formatMemMib(sharedUsedMiB))
                                         .arg(formatMemMib(sharedTotalMiB)));
    this->m_driverValueLabel->setText(this->m_provider->gpuDriverVersion(this->m_gpuIndex));
    this->m_backendValueLabel->setText(this->m_provider->gpuBackendName(this->m_gpuIndex));

    this->rebuildEngineSelectors();

    for (int slot = 0; slot < this->m_engineGraphs.size(); ++slot)
    {
        const int engineIndex = (slot < this->m_selectedEngineBySlot.size())
                                ? this->m_selectedEngineBySlot.at(slot)
                                : -1;
        GraphWidget *graph = this->m_engineGraphs.at(slot);
        QLabel *value = this->m_engineValueLabels.at(slot);

        if (engineIndex >= 0)
        {
            const QString engName = this->m_provider->gpuEngineName(this->m_gpuIndex, engineIndex);
            graph->setSeriesNames(engName);
            graph->setHistory(this->m_provider->gpuEngineHistory(this->m_gpuIndex, engineIndex), 100.0);
            value->setText(QString::number(this->m_provider->gpuEnginePercent(this->m_gpuIndex, engineIndex), 'f', 0) + "%");
        }
        else
        {
            graph->setHistory(QVector<double>(), 100.0);
            value->setText("0%");
        }
    }

    this->m_dedicatedMemGraph->setHistory(this->m_provider->gpuMemUsageHistory(this->m_gpuIndex), 100.0);
    this->m_dedicatedMemGraphMaxLabel->setText(formatMemMib(dedicatedTotalMiB));

    this->m_sharedMemGraph->setHistory(this->m_sharedMemHistory, 100.0);
    this->m_sharedMemGraphMaxLabel->setText(formatMemMib(sharedTotalMiB));

    const QVector<double> &txHistory = this->m_provider->gpuCopyTxHistory(this->m_gpuIndex);
    const QVector<double> &rxHistory = this->m_provider->gpuCopyRxHistory(this->m_gpuIndex);
    double maxCopyRate = 1024.0;
    for (double v : txHistory)
        maxCopyRate = std::max(maxCopyRate, v);
    for (double v : rxHistory)
        maxCopyRate = std::max(maxCopyRate, v);
    this->m_copyBwGraph->setHistory(txHistory, maxCopyRate);
    this->m_copyBwGraph->setSecondaryHistory(rxHistory);
    this->m_copyBwGraphMaxLabel->setText(formatRate(maxCopyRate));
    this->m_copyBwGraph->setToolTip(tr("Copy bandwidth: light trace = TX, dark trace = RX"));
}

QString GpuDetailWidget::formatMemMib(qint64 mib)
{
    const double gb = static_cast<double>(mib) / 1024.0;
    if (gb >= 100.0)
        return QString::number(gb, 'f', 0) + QObject::tr(" GB");
    if (gb >= 10.0)
        return QString::number(gb, 'f', 1) + QObject::tr(" GB");
    return QString::number(gb, 'f', 2) + QObject::tr(" GB");
}

QString GpuDetailWidget::formatRate(double bytesPerSec)
{
    if (bytesPerSec >= 1024.0 * 1024.0)
        return QString::number(bytesPerSec / (1024.0 * 1024.0), 'f', 1) + QObject::tr(" MB/s");
    return QString::number(bytesPerSec / 1024.0, 'f', 0) + QObject::tr(" KB/s");
}

} // namespace Perf
