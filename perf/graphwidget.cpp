#include "graphwidget.h"

#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>

namespace Perf
{

GraphWidget::GraphWidget(QWidget *parent)
    : QWidget(parent)
{
    // Dark background — matches the Windows Task Manager look
    QPalette pal = this->palette();
    pal.setColor(QPalette::Window, QColor(0x0a, 0x0a, 0x0a));
    this->setPalette(pal);
    this->setAutoFillBackground(true);
}

void GraphWidget::setHistory(const QVector<double> &data, double maxVal)
{
    this->m_data   = data;
    this->m_maxVal = (maxVal > 0.0) ? maxVal : 100.0;
    this->update();
}

void GraphWidget::setSecondaryHistory(const QVector<double> &data2)
{
    this->m_data2 = data2;
    this->update();
}

void GraphWidget::setColor(QColor line, QColor fill)
{
    this->m_lineColor = line;
    this->m_fillColor = fill;
    this->update();
}

void GraphWidget::paintEvent(QPaintEvent * /*event*/)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRect r = this->rect();
    const int   w = r.width();
    const int   h = r.height();

    // ── Background ────────────────────────────────────────────────────────────
    p.fillRect(r, QColor(0x0a, 0x0a, 0x0a));

    // ── Grid ──────────────────────────────────────────────────────────────────
    const QColor gridColor(0x28, 0x28, 0x28);
    p.setPen(QPen(gridColor, 1));

    // Horizontal lines
    for (int i = 1; i < this->m_gridRows; ++i)
    {
        const int y = h * i / this->m_gridRows;
        p.drawLine(0, y, w, y);
    }
    // Vertical lines
    for (int i = 1; i < this->m_gridCols; ++i)
    {
        const int x = w * i / this->m_gridCols;
        p.drawLine(x, 0, x, h);
    }

    // ── Data ──────────────────────────────────────────────────────────────────
    if (this->m_data.isEmpty())
        return;

    const int   n      = this->m_data.size();
    const double stepX = static_cast<double>(w) / (n - 1 > 0 ? n - 1 : 1);

    // Build path — left to right, newest sample on the right
    QPainterPath path;
    for (int i = 0; i < n; ++i)
    {
        const double val = qBound(0.0, this->m_data.at(i), this->m_maxVal);
        const double fx  = i * stepX;
        const double fy  = h - (val / this->m_maxVal) * h;

        if (i == 0)
            path.moveTo(fx, fy);
        else
            path.lineTo(fx, fy);
    }

    // Filled area below the line (total user+kernel)
    QPainterPath fillPath = path;
    fillPath.lineTo((n - 1) * stepX, h);
    fillPath.lineTo(0.0, h);
    fillPath.closeSubpath();

    p.setPen(Qt::NoPen);
    p.setBrush(this->m_fillColor);
    p.drawPath(fillPath);

    // Kernel-time overlay (secondary data2) — drawn on top as a darker fill
    if (!this->m_data2.isEmpty())
    {
        const int n2 = qMin(this->m_data2.size(), n);
        // Offset so that the rightmost sample of data2 aligns with data
        const int off = n - n2;
        QPainterPath kPath;
        for (int i = 0; i < n2; ++i)
        {
            const double val = qBound(0.0, this->m_data2.at(i), this->m_maxVal);
            const double fx  = (off + i) * stepX;
            const double fy  = h - (val / this->m_maxVal) * h;
            if (i == 0)
                kPath.moveTo(fx, fy);
            else
                kPath.lineTo(fx, fy);
        }
        QPainterPath kFill = kPath;
        kFill.lineTo((off + n2 - 1) * stepX, h);
        kFill.lineTo(off * stepX, h);
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
}

} // namespace Perf
