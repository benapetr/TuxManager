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

#ifndef PERF_MEMORYBAR_H
#define PERF_MEMORYBAR_H

#include <QPoint>
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
            void SetSegments(qint64 used, qint64 dirty, qint64 cached, qint64 free, qint64 total);

            QSize sizeHint()        const override { return QSize(400, 22); }
            QSize minimumSizeHint() const override { return QSize(80,  12); }

        protected:
            void paintEvent(QPaintEvent *event) override;
            bool event(QEvent *event) override;

        private:
            enum class Segment { None, Used, Dirty, Cached, Free };

            //! Calculates widths for individual segments based on usage so that they fill entire rect
            void    segmentWidths(const QRect &r, int &wUsed, int &wDirty, int &wCached, int &wFree) const;
            //! Used by tooltip - calculates which segment is the mouse pointer hovering
            Segment segmentAtPos(const QPoint &pos) const;
            QString segmentTooltip(Segment seg) const;

            qint64 m_used   { 0 };
            qint64 m_dirty  { 0 };
            qint64 m_cached { 0 };
            qint64 m_free   { 0 };
            qint64 m_total  { 1 };
    };
} // namespace Perf

#endif // PERF_MEMORYBAR_H
