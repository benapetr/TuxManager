#ifndef PERF_MEMORYBAR_H
#define PERF_MEMORYBAR_H

#include <QWidget>

namespace Perf
{

/// Horizontal segmented bar showing memory composition.
///
/// Four segments are drawn left→right to fill the full widget width:
///   1. Used   — processes' non-reclaimable footprint  (bright purple)
///   2. Dirty  — Dirty + Writeback pages               (amber)
///   3. Cached — reclaimable page cache (clean part)   (muted purple)
///   4. Free   — MemFree                               (near-background)
///
/// All values are in any consistent unit (e.g. kB). They must satisfy:
///   used + dirty + cached + free == total
/// (cached here is the full cache including buffers; dirty is a subset of it)
class MemoryBar : public QWidget
{
    Q_OBJECT

    public:
        explicit MemoryBar(QWidget *parent = nullptr);

        /// Update all segment values. @p cached includes @p dirty (dirty is its subset).
        void setSegments(qint64 used, qint64 dirty, qint64 cached,
                         qint64 free, qint64 total);

        QSize sizeHint()        const override { return QSize(400, 22); }
        QSize minimumSizeHint() const override { return QSize(80,  12); }

    protected:
        void paintEvent(QPaintEvent *event) override;

    private:
        qint64 m_used   { 0 };
        qint64 m_dirty  { 0 };
        qint64 m_cached { 0 };
        qint64 m_free   { 0 };
        qint64 m_total  { 1 };
};

} // namespace Perf

#endif // PERF_MEMORYBAR_H
