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

#include "memorybar.h"
#include "../colorscheme.h"
#include "../misc.h"

#include <QEvent>
#include <QHelpEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QToolTip>

using namespace Perf;

MemoryBar::MemoryBar(QWidget *parent) : QWidget(parent)
{
    this->setMinimumHeight(40);
    this->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    this->setMouseTracking(true);
}

void MemoryBar::SetSegments(qint64 used, qint64 dirty, qint64 cached, qint64 free, qint64 total)
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
    const ColorScheme *scheme = ColorScheme::GetCurrent();
    QPainter p(this);
    const QRect r = this->rect().adjusted(0, 0, -1, -1);   // leave 1px for border
    int wUsed = 0, wDirty = 0, wCached = 0, wFree = 0;
    this->segmentWidths(r, wUsed, wDirty, wCached, wFree);

    int x = r.left();
    const int y = r.top();
    const int h = r.height();

    auto drawSeg = [&](int sw, const QColor &col)
    {
        if (sw <= 0) return;
        p.fillRect(x, y, sw, h, col);
        x += sw;
    };

    drawSeg(wUsed,   scheme->MemoryBarUsedColor);
    drawSeg(wDirty,  scheme->MemoryBarDirtyColor);
    drawSeg(wCached, scheme->MemoryBarCachedColor);
    drawSeg(wFree,   scheme->MemoryBarFreeColor);

    // Border
    p.setPen(QPen(scheme->MemoryBarBorderColor, 1));
    p.setBrush(Qt::NoBrush);
    p.drawRect(this->rect().adjusted(0, 0, -1, -1));
}

bool MemoryBar::event(QEvent *event)
{
    if (event->type() == QEvent::ToolTip)
    {
        auto *he = static_cast<QHelpEvent *>(event);
        const Segment seg = this->segmentAtPos(he->pos());
        if (seg == Segment::None)
        {
            QToolTip::hideText();
            event->ignore();
            return true;
        }
        QToolTip::showText(he->globalPos(), this->segmentTooltip(seg), this);
        return true;
    }
    return QWidget::event(event);
}

void MemoryBar::segmentWidths(const QRect &r, int &wUsed, int &wDirty, int &wCached, int &wFree) const
{
    const double w = static_cast<double>(r.width());
    const double t = static_cast<double>(this->m_total);

    const auto segW = [&](qint64 val) -> int {
        return static_cast<int>(static_cast<double>(val) / t * w + 0.5);
    };

    wUsed   = segW(this->m_used);
    wDirty  = segW(this->m_dirty);
    // Clean cache = full cache − dirty
    wCached = segW(qMax(0LL, this->m_cached - this->m_dirty));
    wFree   = r.width() - wUsed - wDirty - wCached;  // absorbs rounding
}

MemoryBar::Segment MemoryBar::segmentAtPos(const QPoint &pos) const
{
    const QRect r = this->rect().adjusted(0, 0, -1, -1);
    if (!r.contains(pos))
        return Segment::None;

    int wUsed = 0, wDirty = 0, wCached = 0, wFree = 0;
    this->segmentWidths(r, wUsed, wDirty, wCached, wFree);

    const int x = pos.x() - r.left();
    if (x < wUsed)
        return Segment::Used;
    if (x < wUsed + wDirty)
        return Segment::Dirty;
    if (x < wUsed + wDirty + wCached)
        return Segment::Cached;
    if (x < wUsed + wDirty + wCached + wFree)
        return Segment::Free;
    return Segment::None;
}

QString MemoryBar::segmentTooltip(Segment seg) const
{
    qint64 value = 0;
    QString label;

    switch (seg)
    {
        case Segment::Used:
            value = this->m_used;
            label = tr("Used");
            break;
        case Segment::Dirty:
            value = this->m_dirty;
            label = tr("Dirty");
            break;
        case Segment::Cached:
            value = qMax(0LL, this->m_cached - this->m_dirty);
            label = tr("Free (cached)");
            break;
        case Segment::Free:
            value = this->m_free;
            label = tr("Free");
            break;
        case Segment::None:
            break;
    }

    const double pct = (this->m_total > 0) ? static_cast<double>(value) * 100.0 / static_cast<double>(this->m_total) : 0.0;
    return tr("%1: %2 (%3%)").arg(label, Misc::FormatKiB(static_cast<quint64>(qMax<qint64>(0, value)), 1), QString::number(pct, 'f', 1));
}
