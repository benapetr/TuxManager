#include "diskdetailwidget.h"
#include "ui_diskdetailwidget.h"

#include <algorithm>

namespace Perf
{

DiskDetailWidget::DiskDetailWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::DiskDetailWidget)
{
    this->ui->setupUi(this);

    // Active time graph
    this->ui->activeGraphWidget->setColor(QColor(0x66, 0xbb, 0x44),
                                          QColor(0x33, 0x66, 0x22, 120));
    this->ui->activeGraphWidget->setSampleCapacity(HISTORY_SIZE);
    this->ui->activeGraphWidget->setGridColumns(6);
    this->ui->activeGraphWidget->setGridRows(4);
    this->ui->activeGraphWidget->setSeriesNames(tr("Active time"));
    this->ui->activeGraphWidget->setValueFormat(GraphWidget::ValueFormat::Percent);

    // Transfer graph (read + write overlay)
    this->ui->transferGraphWidget->setColor(QColor(0x88, 0xcc, 0x66),
                                            QColor(0x33, 0x66, 0x22, 100));
    this->ui->transferGraphWidget->setSampleCapacity(HISTORY_SIZE);
    this->ui->transferGraphWidget->setGridColumns(6);
    this->ui->transferGraphWidget->setGridRows(4);
    this->ui->transferGraphWidget->setSeriesNames(tr("Read"), tr("Write"));
    this->ui->transferGraphWidget->setValueFormat(GraphWidget::ValueFormat::BytesPerSec);
}

DiskDetailWidget::~DiskDetailWidget()
{
    delete this->ui;
}

void DiskDetailWidget::setProvider(PerfDataProvider *provider)
{
    if (this->m_provider)
        disconnect(this->m_provider, &PerfDataProvider::updated,
                   this, &DiskDetailWidget::onUpdated);

    this->m_provider = provider;

    if (this->m_provider)
    {
        connect(this->m_provider, &PerfDataProvider::updated,
                this, &DiskDetailWidget::onUpdated);
        this->onUpdated();
    }
}

void DiskDetailWidget::setDiskIndex(int index)
{
    this->m_diskIndex = index;
    this->onUpdated();
}

void DiskDetailWidget::onUpdated()
{
    if (!this->m_provider || this->m_diskIndex < 0 || this->m_diskIndex >= this->m_provider->diskCount())
        return;

    const QString name = this->m_provider->diskName(this->m_diskIndex);
    const QString model = this->m_provider->diskModel(this->m_diskIndex);
    const QString type = this->m_provider->diskType(this->m_diskIndex);
    const double active = this->m_provider->diskActivePercent(this->m_diskIndex);
    const double readBps = this->m_provider->diskReadBytesPerSec(this->m_diskIndex);
    const double writeBps = this->m_provider->diskWriteBytesPerSec(this->m_diskIndex);
    const qint64 capacityBytes = this->m_provider->diskCapacityBytes(this->m_diskIndex);
    const qint64 formattedBytes = this->m_provider->diskFormattedBytes(this->m_diskIndex);
    const bool isSystemDisk = this->m_provider->diskIsSystemDisk(this->m_diskIndex);
    const bool hasPageFile = this->m_provider->diskHasPageFile(this->m_diskIndex);
    const QVector<double> &activeHistory = this->m_provider->diskActiveHistory(this->m_diskIndex);
    const QVector<double> &readHistory = this->m_provider->diskReadHistory(this->m_diskIndex);
    const QVector<double> &writeHistory = this->m_provider->diskWriteHistory(this->m_diskIndex);

    this->ui->titleLabel->setText(tr("Disk (%1)").arg(name));
    this->ui->modelLabel->setText(model);

    this->ui->activeValueLabel->setText(QString::number(active, 'f', 0) + "%");
    this->ui->readValueLabel->setText(formatRate(readBps));
    this->ui->writeValueLabel->setText(formatRate(writeBps));
    this->ui->typeValueLabel->setText(type);
    this->ui->deviceValueLabel->setText("/dev/" + name);
    this->ui->capacityValueLabel->setText(formatSize(capacityBytes));
    this->ui->formattedValueLabel->setText(formattedBytes > 0 ? formatSize(formattedBytes) : tr("—"));
    this->ui->systemDiskValueLabel->setText(isSystemDisk ? tr("Yes") : tr("No"));
    this->ui->pageFileValueLabel->setText(hasPageFile ? tr("Yes") : tr("No"));

    this->ui->activeGraphWidget->setHistory(activeHistory, 100.0);
    this->ui->activeGraphMaxLabel->setText(tr("100%"));

    double maxRate = 1024.0; // at least 1 KB/s scale
    for (double v : readHistory)
        maxRate = std::max(maxRate, v);
    for (double v : writeHistory)
        maxRate = std::max(maxRate, v);
    this->ui->transferGraphWidget->setHistory(readHistory, maxRate);
    this->ui->transferGraphWidget->setSecondaryHistory(writeHistory);
    this->ui->transferGraphMaxLabel->setText(formatRate(maxRate));
}

QString DiskDetailWidget::formatRate(double bytesPerSec)
{
    if (bytesPerSec >= 1024.0 * 1024.0)
        return QString::number(bytesPerSec / (1024.0 * 1024.0), 'f', 1) + tr(" MB/s");
    return QString::number(bytesPerSec / 1024.0, 'f', 0) + tr(" KB/s");
}

QString DiskDetailWidget::formatSize(qint64 bytes)
{
    if (bytes <= 0)
        return tr("0 GB");
    const double gb = static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0);
    if (gb >= 100.0)
        return QString::number(gb, 'f', 0) + tr(" GB");
    if (gb >= 10.0)
        return QString::number(gb, 'f', 1) + tr(" GB");
    return QString::number(gb, 'f', 2) + tr(" GB");
}

} // namespace Perf
