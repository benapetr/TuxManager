#include "memorybar.h"

#include <QPainter>
#include <QPaintEvent>

namespace Perf
{

// Segment colours (dark-theme)
static const QColor kColUsed  (0xcc, 0x44, 0xcc);        // bright purple — matches graph
static const QColor kColDirty (0xbb, 0x88, 0x00);        // amber
static const QColor kColCached(0x55, 0x22, 0x55);        // muted dark purple
static const QColor kColFree  (0x11, 0x08, 0x11);        // near-background
static const QColor kColBorder(0x88, 0x44, 0x88);        // subtle purple border

MemoryBar::MemoryBar(QWidget *parent)
    : QWidget(parent)
{
    this->setMinimumHeight(12);
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void MemoryBar::setSegments(qint64 used, qint64 dirty,
                             qint64 cached, qint64 free, qint64 total)
{
    this->m_used   = qMax(0LL, used);
    this->m_dirty  = qMax(0LL, dirty);
    this->m_cached = qMax(0LL, cached);
    this->m_free   = qMax(0LL, free);
    this->m_total  = (total > 0) ? total : 1;
    this->update();
}

void MemoryBar::paintEvent(QPaintEvent * /*event*/)
{
    QPainter p(this);
    const QRect r = this->rect().adjusted(0, 0, -1, -1);   // leave 1px for border
    const double w = static_cast<double>(r.width());
    const double t = static_cast<double>(this->m_total);

    // Calculate pixel widths for each segment (ensure they sum exactly to r.width())
    const auto segW = [&](qint64 val) -> int {
        return static_cast<int>(static_cast<double>(val) / t * w + 0.5);
    };

    int wUsed   = segW(this->m_used);
    int wDirty  = segW(this->m_dirty);
    // Clean cache = full cache − dirty
    int wCached = segW(qMax(0LL, this->m_cached - this->m_dirty));
    int wFree   = r.width() - wUsed - wDirty - wCached;  // absorbs rounding

    int x = r.left();
    const int y = r.top();
    const int h = r.height();

    auto drawSeg = [&](int sw, const QColor &col) {
        if (sw <= 0) return;
        p.fillRect(x, y, sw, h, col);
        x += sw;
    };

    drawSeg(wUsed,   kColUsed);
    drawSeg(wDirty,  kColDirty);
    drawSeg(wCached, kColCached);
    drawSeg(wFree,   kColFree);

    // Border
    p.setPen(QPen(kColBorder, 1));
    p.setBrush(Qt::NoBrush);
    p.drawRect(this->rect().adjusted(0, 0, -1, -1));
}

} // namespace Perf
