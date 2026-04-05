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

#include "sidepanelitem.h"
#include "perfdataprovider.h"

#include <QPainter>
#include <QPaintEvent>
#include <QVBoxLayout>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QEnterEvent>
#endif

using namespace Perf;

SidePanelItem::SidePanelItem(const QString &title, QWidget *parent) : QWidget(parent), m_title(title), m_graph(new GraphWidget(this))
{
    this->setCursor(Qt::PointingHandCursor);
    this->setMouseTracking(true);

    // Layout: the graph fills most of the cell; title/subtitle are painted
    // directly in paintEvent to avoid font layout overhead.
    QVBoxLayout *lay = new QVBoxLayout(this);
    lay->setContentsMargins(6, 22, 6, 4);   // leave room for title text
    lay->setSpacing(0);
    this->m_graph->SetSampleCapacity(HISTORY_SIZE);
    this->m_graph->SetHoverLineEnabled(false);
    this->m_graph->SetHoverTooltipEnabled(false);
    lay->addWidget(this->m_graph);
    this->setLayout(lay);
}

void SidePanelItem::Update(const QString &subtitle, const QVector<double> &history, double maxVal)
{
    this->m_subtitle = subtitle;
    this->m_graph->SetHistoryRef(history, maxVal);
    this->repaint();  // repaint own text; graph repaints itself inside setHistory
}

void SidePanelItem::SetSelected(bool selected)
{
    if (this->m_selected == selected)
        return;
    this->m_selected = selected;
    this->repaint();
}

void SidePanelItem::SetGraphColor(QColor line, QColor fill)
{
    this->m_graph->SetColor(line, fill);
}

// ── Paint ─────────────────────────────────────────────────────────────────────

void SidePanelItem::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);   // draw children (the graph widget)

    QPainter p(this);
    const QPalette pal = this->palette();
    const bool darkTheme = pal.color(QPalette::Window).lightness() <= 127;

    const QRect r = this->rect();

    // Background
    QColor bg;
    if (this->m_selected)
        bg = darkTheme ? QColor(0x1a, 0x4a, 0x8a, 200)
                       : QColor(pal.color(QPalette::Highlight).red(),
                                pal.color(QPalette::Highlight).green(),
                                pal.color(QPalette::Highlight).blue(),
                                135);
    else if (this->m_hovered)
        bg = darkTheme ? QColor(0x30, 0x30, 0x40, 120)
                       : QColor(0, 0, 0, 20);
    else
        bg = darkTheme ? QColor(0x1a, 0x1a, 0x22, 180)
                       : pal.color(QPalette::Window);

    p.fillRect(r, bg);

    // Title/subtitle (single row): elide both sides to prevent overlap.
    QFont titleFont = this->font();
    titleFont.setBold(true);
    titleFont.setPointSize(8);
    const QFontMetrics titleFm(titleFont);
    QFont subFont = this->font();
    subFont.setPointSize(7);
    const QFontMetrics subFm(subFont);

    const int left = 6;
    const int right = 6;
    const int top = 4;
    const int textH = 16;
    const int fullW = qMax(0, r.width() - left - right);

    // Allow a longer subtitle while keeping enough room for a readable title.
    const int maxSubW = (fullW * 58) / 100;
    const QString subText = subFm.elidedText(this->m_subtitle, Qt::ElideLeft, maxSubW);
    const int subW = subFm.horizontalAdvance(subText);

    const int titleMaxW = qMax(0, fullW - (subW > 0 ? subW + 6 : 0));
    const QString titleText = titleFm.elidedText(this->m_title, Qt::ElideRight, titleMaxW);

    p.setFont(titleFont);
    p.setPen(darkTheme ? (this->m_selected ? Qt::white : QColor(0xcc, 0xcc, 0xcc))
                       : (this->m_selected ? pal.color(QPalette::HighlightedText) : pal.color(QPalette::WindowText)));
    p.drawText(QRect(left, top, titleMaxW, textH),
               Qt::AlignLeft | Qt::AlignVCenter,
               titleText);

    if (!subText.isEmpty())
    {
        p.setFont(subFont);
        p.setPen(darkTheme ? QColor(0x88, 0xaa, 0xcc) : pal.color(QPalette::Mid));
        p.drawText(QRect(r.width() - right - subW, top, subW, textH),
                   Qt::AlignRight | Qt::AlignVCenter,
                   subText);
    }

    // Selection border
    if (this->m_selected)
    {
        p.setPen(QPen(darkTheme ? QColor(0x44, 0x88, 0xff) : pal.color(QPalette::Highlight), 2));
        p.setBrush(Qt::NoBrush);
        p.drawRect(r.adjusted(1, 1, -1, -1));
    }
}

// ── Events ────────────────────────────────────────────────────────────────────

void SidePanelItem::mousePressEvent(QMouseEvent *event)
{
    QWidget::mousePressEvent(event);
    if (event->button() == Qt::LeftButton)
        emit this->clicked();
    else if (event->button() == Qt::RightButton)
    {
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        emit this->contextMenuRequested(event->globalPosition().toPoint());
#else
        emit this->contextMenuRequested(event->globalPos());
#endif
    }
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void SidePanelItem::enterEvent(QEnterEvent *event)
#else
void SidePanelItem::enterEvent(QEvent *event)
#endif
{
    QWidget::enterEvent(event);
    this->m_hovered = true;
    this->repaint();
}

void SidePanelItem::leaveEvent(QEvent *event)
{
    QWidget::leaveEvent(event);
    this->m_hovered = false;
    this->repaint();
}
