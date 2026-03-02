#include "memorydetailwidget.h"
#include "ui_memorydetailwidget.h"

namespace Perf
{

MemoryDetailWidget::MemoryDetailWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::MemoryDetailWidget)
{
    this->ui->setupUi(this);

    // Memory graph: purple / magenta
    this->ui->graphWidget->setColor(QColor(0xcc, 0x44, 0xcc),
                                    QColor(0x66, 0x11, 0x66, 130));
    this->ui->graphWidget->setGridColumns(6);
    this->ui->graphWidget->setGridRows(4);
}

MemoryDetailWidget::~MemoryDetailWidget()
{
    delete this->ui;
}

void MemoryDetailWidget::setProvider(PerfDataProvider *provider)
{
    if (this->m_provider)
        disconnect(this->m_provider, &PerfDataProvider::updated,
                   this, &MemoryDetailWidget::onUpdated);

    this->m_provider = provider;

    if (this->m_provider)
    {
        connect(this->m_provider, &PerfDataProvider::updated,
                this, &MemoryDetailWidget::onUpdated);
        this->onUpdated();
    }
}

void MemoryDetailWidget::onUpdated()
{
    if (!this->m_provider)
        return;

    const qint64 total   = this->m_provider->memTotalKb();
    const qint64 used    = this->m_provider->memUsedKb();
    const qint64 avail   = this->m_provider->memAvailKb();
    const qint64 free    = this->m_provider->memFreeKb();
    const qint64 cached  = this->m_provider->memCachedKb();   // includes buffers
    const qint64 buffers = this->m_provider->memBuffersKb();
    const qint64 dirty   = this->m_provider->memDirtyKb();

    this->ui->totalLabel->setText(fmtGb(total) + " GB");
    this->ui->statInUseValue->setText(fmtGb(used)    + " GB");
    this->ui->statAvailValue->setText(fmtGb(avail)   + " GB");
    this->ui->statCachedValue->setText(fmtGb(cached) + " GB");
    this->ui->statBuffersValue->setText(fmtGb(buffers) + " GB");
    this->ui->statFreeValue->setText(fmtGb(free)     + " GB");

    // Dirty shown in MB when small, GB when large
    if (dirty < 1024LL * 1024LL)
        this->ui->statDirtyValue->setText(
                QString::number(dirty / 1024.0, 'f', 1) + " MB");
    else
        this->ui->statDirtyValue->setText(fmtGb(dirty) + " GB");

    // Composition bar — 4 segments must sum to total
    // free   = MemFree
    // cached = Buffers + PageCache  (includes dirty subset)
    // used   = Total - Free - Cached (htop formula, non-reclaimable)
    // Verify: used + cached + free == total  ✓
    this->ui->compositionBar->setSegments(used, dirty, cached, free, total);

    this->ui->graphWidget->setHistory(this->m_provider->memHistory());
}

// static
QString MemoryDetailWidget::fmtGb(qint64 kb)
{
    const double gb = static_cast<double>(kb) / (1024.0 * 1024.0);
    if (gb >= 10.0)
        return QString::number(gb, 'f', 1);
    return QString::number(gb, 'f', 2);
}

} // namespace Perf
