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
    this->setAutoFillBackground(false);
    this->setMouseTracking(true);
}

void GraphWidget::SetHistory(const QVector<double> &data, double maxVal)
{
    // Advance grid phase on every sample tick, even if values are unchanged.
    // This keeps the time axis moving continuously (task-manager behavior).
    ++this->m_historyTick;
    this->m_dataRef = nullptr;
    this->m_data   = data;
    this->m_maxVal = (maxVal > 0.0) ? maxVal : 100.0;
    this->update();
}

void GraphWidget::SetHistoryRef(const QVector<double> &data, double maxVal)
{
    ++this->m_historyTick;
    this->m_dataRef = &data;
    this->m_maxVal = (maxVal > 0.0) ? maxVal : 100.0;
    this->update();
}

void GraphWidget::SetSecondaryHistory(const QVector<double> &data2)
{
    this->m_data2Ref = nullptr;
    this->m_data2 = data2;
    this->update();
}

void GraphWidget::SetSecondaryHistoryRef(const QVector<double> &data2)
{
    this->m_data2Ref = &data2;
    this->update();
}

void GraphWidget::SetSeriesNames(const QString &primary, const QString &secondary)
{
    if (!primary.isEmpty())
        this->m_primaryName = primary;
    if (!secondary.isEmpty())
        this->m_secondaryName = secondary;
}

void GraphWidget::SetColor(QColor line, QColor fill)
{
    this->m_lineColor = line;
    this->m_fillColor = fill;
    this->update();
}

void GraphWidget::SetSampleCapacity(int samples)
{
    this->m_sampleCapacity = qMax(2, samples);
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

    // ── Background ────────────────────────────────────────────────────────────
    const QPalette pal = this->palette();
    const QColor bg = pal.color(QPalette::Base);
    const bool darkTheme = bg.lightness() <= 127;
    p.fillRect(r, bg);

    // Fixed time axis slot geometry.
    const int sampleCount = qMax(2, this->m_sampleCapacity);
    const double stepX = static_cast<double>(w) / static_cast<double>(sampleCount - 1);

    // ── Grid ──────────────────────────────────────────────────────────────────
    QColor gridColor = darkTheme ? pal.color(QPalette::Midlight)
                                 : pal.color(QPalette::Text);
    if (gridColor.alpha() == 255)
        gridColor.setAlpha(darkTheme ? 150 : 62);
    p.setPen(QPen(gridColor, 1));

    // Denser grid on larger widgets while keeping existing configured minimum.
    const int targetGridPxX = 80;
    const int targetGridPxY = 55;
    const int gridCols = qMax(this->m_gridCols, qMax(1, w / targetGridPxX));
    const int gridRows = qMax(this->m_gridRows, qMax(1, h / targetGridPxY));

    // Horizontal lines
    for (int i = 1; i < gridRows; ++i)
    {
        const int y = h * i / gridRows;
        p.drawLine(0, y, w, y);
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

        const int x = static_cast<int>(slot * stepX + 0.5);
        if (x <= 0 || x >= w || x == lastX)
            continue;
        p.drawLine(x, 0, x, h);
        lastX = x;
    }

    // ── Data ──────────────────────────────────────────────────────────────────
    const QVector<double> *primary = this->primarySource();
    if (!primary || primary->isEmpty())
        return;

    const int n = primary->size();

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
        const double val = qBound(0.0, primary->at(visibleStart + i), this->m_maxVal);
        const double fx  = (slotOffset + i) * stepX;
        const double fy  = h - (val / this->m_maxVal) * h;

        if (i == 0)
            path.moveTo(fx, fy);
        else
            path.lineTo(fx, fy);
    }

    // Filled area below the line (total user+kernel)
    QPainterPath fillPath = path;
    fillPath.lineTo((slotOffset + visibleCount - 1) * stepX, h);
    fillPath.lineTo(slotOffset * stepX, h);
    fillPath.closeSubpath();

    p.setPen(Qt::NoPen);
    p.setBrush(this->m_fillColor);
    p.drawPath(fillPath);

    // Kernel-time overlay (secondary data2) — drawn on top as a darker fill
    const QVector<double> *secondary = this->secondarySource();
    if (secondary && !secondary->isEmpty())
    {
        const int n2 = secondary->size();
        const int visibleStart2 = qMax(0, n2 - sampleCount);
        const int visibleCount2 = n2 - visibleStart2;
        const int slotOffset2 = qMax(0, sampleCount - visibleCount2);
        QPainterPath kPath;
        for (int i = 0; i < visibleCount2; ++i)
        {
            const double val = qBound(0.0, secondary->at(visibleStart2 + i), this->m_maxVal);
            const double fx  = (slotOffset2 + i) * stepX;
            const double fy  = h - (val / this->m_maxVal) * h;
            if (i == 0)
                kPath.moveTo(fx, fy);
            else
                kPath.lineTo(fx, fy);
        }
        QPainterPath kFill = kPath;
        kFill.lineTo((slotOffset2 + visibleCount2 - 1) * stepX, h);
        kFill.lineTo(slotOffset2 * stepX, h);
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
    p.drawRect(r.adjusted(0, 0, -1, -1));

    if (!this->m_overlayText.isEmpty())
    {
        QFont f = p.font();
        f.setPointSizeF(qMax(7.0, f.pointSizeF() - 1.0));
        p.setFont(f);
        p.setPen(darkTheme ? QColor(245, 245, 245, 220)
                           : QColor(35, 35, 35, 220));
        p.drawText(r.adjusted(4, 2, -4, -2),
                   Qt::AlignLeft | Qt::AlignTop,
                   this->m_overlayText);
    }

    if (this->m_hoverLineEnabled && this->m_hoverSlot >= 0 && this->m_hoverSlot < sampleCount)
    {
        const int x = static_cast<int>(this->m_hoverSlot * stepX + 0.5);
        QColor hover = this->m_lineColor;
        hover.setAlpha(170);
        p.setPen(QPen(hover, 1));
        p.drawLine(x, 0, x, h);
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

    if (slot != this->m_hoverSlot)
    {
        this->m_hoverSlot = slot;
        this->update();
    }

    if (this->m_hoverTooltipEnabled)
    {
        const QVector<double> *primary = this->primarySource();
        const QVector<double> *secondary = this->secondarySource();
        const int idx1 = sampleIndexForSlot(primary ? primary->size() : 0, slot, sampleCount);
        const int idx2 = sampleIndexForSlot(secondary ? secondary->size() : 0, slot, sampleCount);

        if (idx1 >= 0 || idx2 >= 0)
        {
            QString tip;
            if (idx1 >= 0)
                tip += tr("%1: %2").arg(this->m_primaryName, this->formatValue(primary->at(idx1)));
            if (idx2 >= 0)
            {
                if (!tip.isEmpty())
                    tip += "\n";
                tip += tr("%1: %2").arg(this->m_secondaryName, this->formatValue(secondary->at(idx2)));
            }
            const int secAgo = sampleCount - 1 - slot;
            if (!tip.isEmpty())
                tip += tr("\n%1 s ago").arg(secAgo);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            QToolTip::showText(event->globalPosition().toPoint(), tip, this);
#else
            QToolTip::showText(event->globalPos(), tip, this);
#endif
        }
        else
        {
            QToolTip::hideText();
        }
    }

    QWidget::mouseMoveEvent(event);
}

void GraphWidget::leaveEvent(QEvent *event)
{
    this->m_hoverSlot = -1;
    if (this->m_hoverTooltipEnabled)
        QToolTip::hideText();
    this->update();
    QWidget::leaveEvent(event);
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

const QVector<double> *GraphWidget::primarySource() const
{
    return this->m_dataRef ? this->m_dataRef : &this->m_data;
}

const QVector<double> *GraphWidget::secondarySource() const
{
    return this->m_data2Ref ? this->m_data2Ref : &this->m_data2;
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
            return tr("%1 (%2 %3)")
                    .arg(percent)
                    .arg(QString::number(absolute, 'f', this->m_percentTooltipAbsolutePrecision))
                    .arg(this->m_percentTooltipAbsoluteUnit);
        }
        case ValueFormat::BytesPerSec:
            if (v >= 1024.0 * 1024.0 * 1024.0)
                return QString::number(v / (1024.0 * 1024.0 * 1024.0), 'f', 2) + tr(" GB/s");
            if (v >= 1024.0 * 1024.0)
                return QString::number(v / (1024.0 * 1024.0), 'f', 1) + tr(" MB/s");
            if (v >= 1024.0)
                return QString::number(v / 1024.0, 'f', 0) + tr(" KB/s");
            return QString::number(v, 'f', 0) + tr(" B/s");
        case ValueFormat::Raw:
            return QString::number(v, 'f', 2);
        case ValueFormat::Auto:
        default:
            if (this->m_maxVal <= 100.0)
                return QString::number(v, 'f', 1) + tr("%");
            return QString::number(v, 'f', 2);
    }
}
