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

#include "graphwidget.h"
#include "../colorscheme.h"
#include "misc.h"

#include <cmath>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QToolTip>
#include <QtGlobal>

using namespace Perf;

GraphWidget::GraphWidget(QWidget *parent) : QWidget(parent)
{
    const ColorScheme *scheme = ColorScheme::GetCurrent();
    this->m_lineColor = scheme->CpuGraphLineColor;
    this->m_fillColor = scheme->CpuGraphFillColor;
    this->m_fillColor2 = scheme->CpuGraphSecondaryFillColor;
    this->setAutoFillBackground(false);
    this->setMouseTracking(true);
}

void GraphWidget::SetDataSource(const HistoryBuffer &data, double maxVal)
{
    this->m_data = &data;
    this->m_maxVal = (maxVal > 0.0) ? maxVal : 100.0;
    this->update();
}

void GraphWidget::SetOverlayDataSource(const HistoryBuffer &data)
{
    this->m_overlayData = &data;
    this->update();
}

void GraphWidget::SetMax(double maxVal)
{
    this->m_maxVal = (maxVal > 0.0) ? maxVal : 100.0;
}

void GraphWidget::Tick(bool requestUpdate)
{
    ++this->m_historyTick;
    if (requestUpdate)
        this->update();
}

void GraphWidget::ClearDataSource()
{
    this->m_data = nullptr;
}

void GraphWidget::ClearOverlayDataSource()
{
    this->m_overlayData = nullptr;
}

void GraphWidget::SetSeriesNames(const QString &primary, const QString &secondary)
{
    if (!primary.isEmpty())
        this->m_primaryName = primary;
    if (!secondary.isEmpty())
        this->m_secondaryName = secondary;
}

void GraphWidget::SetColor(QColor line, QColor fill, QColor fill2)
{
    this->m_lineColor = line;
    this->m_fillColor = fill;
    this->m_fillColor2 = fill2.isValid() ? fill2 : fill.darker(160);
    this->update();
}

void GraphWidget::SetSampleCapacity(int samples)
{
    this->m_sampleCapacity = qMax(2, samples);
    this->update();
}

void GraphWidget::SetOverlayText(const QString &text)
{
    if (this->m_overlayText == text)
        return;

    this->m_overlayText = text;
    this->update();
}

void GraphWidget::SetPercentTooltipAbsolute(double maxAbsoluteValue, const QString &unitLabel, int precision)
{
    this->m_percentTooltipAbsoluteEnabled = (maxAbsoluteValue > 0.0);
    this->m_percentTooltipAbsoluteMax = qMax(0.0, maxAbsoluteValue);
    this->m_percentTooltipAbsoluteUnit = unitLabel;
    this->m_percentTooltipAbsolutePrecision = qMax(0, precision);
}

void GraphWidget::paintEvent(QPaintEvent * /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRect r = this->rect();
    const int   w = r.width();
    const int   h = r.height();
    const double right = qMax(0, w - 1);
    const double bottom = qMax(0, h - 1);
    const double contentLeft = (w > 2) ? 1.0 : 0.0;
    const double contentTop = (h > 2) ? 1.0 : 0.0;
    const double contentRight = qMax(contentLeft, right - 1.0);
    const double contentBottom = qMax(contentTop, bottom - 1.0);
    const double contentWidth = qMax(0.0, contentRight - contentLeft);
    const double contentHeight = qMax(0.0, contentBottom - contentTop);

    // ── Background ────────────────────────────────────────────────────────────
    const QColor bg = this->palette().color(QPalette::Base);
    const ColorScheme *scheme = ColorScheme::GetCurrent();
    p.fillRect(r, bg);

    // Fixed time axis slot geometry.
    const int sampleCount = qMax(2, this->m_sampleCapacity);
    const double stepX = contentWidth / static_cast<double>(sampleCount - 1);

    // ── Grid ──────────────────────────────────────────────────────────────────
    p.setPen(QPen(scheme->GraphGridColor, 1));

    // Denser grid on larger widgets while keeping existing configured minimum.
    const int targetGridPxX = 80;
    const int targetGridPxY = 55;
    const int gridCols = qMax(this->m_gridCols, qMax(1, w / targetGridPxX));
    const int gridRows = qMax(this->m_gridRows, qMax(1, h / targetGridPxY));

    // Horizontal lines
    for (int i = 1; i < gridRows; ++i)
    {
        const double y = contentTop + contentHeight * i / gridRows;
        p.drawLine(QPointF(contentLeft, y), QPointF(contentRight, y));
    }

    // Vertical lines snap to time slots and phase-shift with sample updates.
    // This keeps graph points aligned to the same grid columns as data scrolls.
    const int gridSlotStep = qMax(1, sampleCount / gridCols);
    const int phase = this->m_historyTick % gridSlotStep;
    int lastX = -1;
    for (int slot = 1; slot < sampleCount - 1; ++slot)
    {
        if (((slot + phase) % gridSlotStep) != 0)
            continue;

        const int x = static_cast<int>(contentLeft + slot * stepX + 0.5);
        if (x <= contentLeft || x >= contentRight || x == lastX)
            continue;
        p.drawLine(QPointF(x, contentTop), QPointF(x, contentBottom));
        lastX = x;
    }

    // ── Data ──────────────────────────────────────────────────────────────────
    if (!this->m_data || this->m_data->IsEmpty())
        return;

    const int n = this->m_data->Size();

    // Keep a fixed-width time axis:
    // - when history is short, right-align it (empty area on the left)
    // - once full, new samples push older ones off the left edge
    const int visibleStart = qMax(0, n - sampleCount);
    const int visibleCount = n - visibleStart;
    const int slotOffset = qMax(0, sampleCount - visibleCount);

    // Build path — left to right, newest sample on the right
    QPainterPath path;
    for (int i = 0; i < visibleCount; ++i)
    {
        const double val = qBound(0.0, this->m_data->At(visibleStart + i), this->m_maxVal);
        const double fx  = contentLeft + (slotOffset + i) * stepX;
        const double fy  = contentBottom - (val / this->m_maxVal) * contentHeight;

        if (i == 0)
            path.moveTo(fx, fy);
        else
            path.lineTo(fx, fy);
    }

    // Filled area below the line (total user+kernel)
    QPainterPath fillPath = path;
    fillPath.lineTo(contentLeft + (slotOffset + visibleCount - 1) * stepX, contentBottom);
    fillPath.lineTo(contentLeft + slotOffset * stepX, contentBottom);
    fillPath.closeSubpath();

    p.setPen(Qt::NoPen);
    p.setBrush(this->m_fillColor);
    p.drawPath(fillPath);

    // Kernel-time overlay (secondary data2) — drawn on top as a darker fill
    if (this->m_overlayData && !this->m_overlayData->IsEmpty())
    {
        const int n2 = this->m_overlayData->Size();
        const int visibleStart2 = qMax(0, n2 - sampleCount);
        const int visibleCount2 = n2 - visibleStart2;
        const int slotOffset2 = qMax(0, sampleCount - visibleCount2);
        QPainterPath kPath;
        for (int i = 0; i < visibleCount2; ++i)
        {
            const double val = qBound(0.0, this->m_overlayData->At(visibleStart2 + i), this->m_maxVal);
            const double fx  = contentLeft + (slotOffset2 + i) * stepX;
            const double fy  = contentBottom - (val / this->m_maxVal) * contentHeight;
            if (i == 0)
                kPath.moveTo(fx, fy);
            else
                kPath.lineTo(fx, fy);
        }
        QPainterPath kFill = kPath;
        kFill.lineTo(contentLeft + (slotOffset2 + visibleCount2 - 1) * stepX, contentBottom);
        kFill.lineTo(contentLeft + slotOffset2 * stepX, contentBottom);
        kFill.closeSubpath();
        p.setPen(Qt::NoPen);
        p.setBrush(this->m_fillColor2);
        p.drawPath(kFill);
    }

    // The line itself
    p.setPen(QPen(this->m_lineColor, 1.5));
    p.setBrush(Qt::NoBrush);
    p.drawPath(path);

    // ── Border ────────────────────────────────────────────────────────────────
    p.setPen(QPen(this->m_lineColor.darker(150), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRect(QRectF(0.5, 0.5, qMax(0.0, right - 1.0), qMax(0.0, bottom - 1.0)));

    if (!this->m_overlayText.isEmpty())
    {
        QFont f = p.font();
        f.setPointSizeF(qMax(7.0, f.pointSizeF() - 1.0));
        p.setFont(f);
        p.setPen(scheme->GraphOverlayTextColor);
        p.drawText(r.adjusted(4, 2, -4, -2), Qt::AlignLeft | Qt::AlignTop, this->m_overlayText);
    }

    if (this->m_hoverLineEnabled && this->m_hoverSlot >= 0 && this->m_hoverSlot < sampleCount)
    {
        const int x = static_cast<int>(contentLeft + this->m_hoverSlot * stepX + 0.5);
        QColor hover = this->m_lineColor;
        hover.setAlpha(170);
        p.setPen(QPen(hover, 1));
        p.drawLine(QPointF(x, contentTop), QPointF(x, contentBottom));
    }
}

void GraphWidget::mouseMoveEvent(QMouseEvent *event)
{
    const int sampleCount = qMax(2, this->m_sampleCapacity);
    const double stepX = static_cast<double>(qMax(1, this->width())) / static_cast<double>(sampleCount - 1);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const double mouseX = event->position().x();
#else
    const double mouseX = event->pos().x();
#endif
    const int slot = qBound(0, static_cast<int>(std::lround(mouseX / stepX)), sampleCount - 1);
    const bool slotChanged = (slot != this->m_hoverSlot);

    if (slotChanged)
    {
        const QRect oldRect = this->hoverLineRect(this->m_hoverSlot);
        this->m_hoverSlot = slot;
        this->update(oldRect.united(this->hoverLineRect(this->m_hoverSlot)));
    }

    if (this->m_hoverTooltipEnabled && slotChanged)
    {
        const int idx1 = sampleIndexForSlot(this->m_data ? this->m_data->Size() : 0, slot, sampleCount);
        const int idx2 = sampleIndexForSlot(this->m_overlayData ? this->m_overlayData->Size() : 0, slot, sampleCount);

        if (idx1 >= 0 || idx2 >= 0)
        {
            QString tip;
            if (idx1 >= 0)
                tip += tr("%1: %2").arg(this->m_primaryName, this->formatValue(this->m_data->At(idx1)));
            if (idx2 >= 0)
            {
                if (!tip.isEmpty())
                    tip += "\n";
                tip += tr("%1: %2").arg(this->m_secondaryName, this->formatValue(this->m_overlayData->At(idx2)));
            }
            const int secAgo = sampleCount - 1 - slot;
            if (!tip.isEmpty())
                tip += tr("\n%1 s ago").arg(secAgo);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            QToolTip::showText(event->globalPosition().toPoint(), tip, this);
#else
            QToolTip::showText(event->globalPos(), tip, this);
#endif
        } else
        {
            QToolTip::hideText();
        }
    }

    QWidget::mouseMoveEvent(event);
}

void GraphWidget::leaveEvent(QEvent *event)
{
    const QRect oldRect = this->hoverLineRect(this->m_hoverSlot);
    this->m_hoverSlot = -1;
    if (this->m_hoverTooltipEnabled)
        QToolTip::hideText();
    this->update(oldRect);
    QWidget::leaveEvent(event);
}

QRect GraphWidget::hoverLineRect(int slot) const
{
    if (!this->m_hoverLineEnabled || slot < 0)
        return {};

    const int sampleCount = qMax(2, this->m_sampleCapacity);
    const int width = qMax(1, this->width());
    const double contentLeft = (width > 2) ? 1.0 : 0.0;
    const double contentRight = qMax(contentLeft, static_cast<double>(width - 2));
    const double stepX = (contentRight - contentLeft) / static_cast<double>(sampleCount - 1);
    const int x = static_cast<int>(contentLeft + slot * stepX + 0.5);
    return QRect(x - 3, 0, 7, this->height()).intersected(this->rect());
}

int GraphWidget::sampleIndexForSlot(int size, int slot, int sampleCount)
{
    if (size <= 0 || slot < 0 || sampleCount < 2)
        return -1;
    const int visibleStart = qMax(0, size - sampleCount);
    const int visibleCount = size - visibleStart;
    const int slotOffset = qMax(0, sampleCount - visibleCount);
    if (slot < slotOffset || slot >= slotOffset + visibleCount)
        return -1;
    return visibleStart + (slot - slotOffset);
}

QString GraphWidget::formatValue(double v) const
{
    switch (this->m_valueFormat)
    {
        case ValueFormat::Percent:
        {
            const QString percent = QString::number(v, 'f', 1) + tr("%");
            if (!this->m_percentTooltipAbsoluteEnabled || this->m_percentTooltipAbsoluteMax <= 0.0)
                return percent;

            const double absolute = (v / 100.0) * this->m_percentTooltipAbsoluteMax;
            return tr("%1 (%2 %3)", "%1=percent text %2=absolute value %3=absolute unit")
                    .arg(percent,
                         QString::number(absolute, 'f', this->m_percentTooltipAbsolutePrecision),
                         this->m_percentTooltipAbsoluteUnit);
        }
        case ValueFormat::BytesPerSec:
            return Misc::FormatBytesPerSecond(v);
        case ValueFormat::BitsPerSec:
            return Misc::FormatBitsPerSecond(v);
        case ValueFormat::Raw:
            return QString::number(v, 'f', 2);
        case ValueFormat::Auto:
        default:
            if (this->m_maxVal <= 100.0)
                return QString::number(v, 'f', 1) + tr("%");
            return QString::number(v, 'f', 2);
    }
}
