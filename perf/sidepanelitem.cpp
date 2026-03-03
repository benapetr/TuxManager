#include "sidepanelitem.h"
#include "perfdataprovider.h"

#include <QEnterEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QVBoxLayout>

namespace Perf
{

SidePanelItem::SidePanelItem(const QString &title, QWidget *parent)
    : QWidget(parent)
    , m_title(title)
    , m_graph(new GraphWidget(this))
{
    this->setCursor(Qt::PointingHandCursor);
    this->setMouseTracking(true);

    // Layout: the graph fills most of the cell; title/subtitle are painted
    // directly in paintEvent to avoid font layout overhead.
    QVBoxLayout *lay = new QVBoxLayout(this);
    lay->setContentsMargins(6, 22, 6, 4);   // leave room for title text
    lay->setSpacing(0);
    this->m_graph->setSampleCapacity(HISTORY_SIZE);
    this->m_graph->setHoverLineEnabled(false);
    this->m_graph->setHoverTooltipEnabled(false);
    lay->addWidget(this->m_graph);
    this->setLayout(lay);
}

void SidePanelItem::update(const QString &subtitle,
                           const QVector<double> &history,
                           double maxVal)
{
    this->m_subtitle = subtitle;
    this->m_graph->setHistory(history, maxVal);
    this->repaint();  // repaint own text; graph repaints itself inside setHistory
}

void SidePanelItem::setSelected(bool selected)
{
    if (this->m_selected == selected)
        return;
    this->m_selected = selected;
    this->repaint();
}

void SidePanelItem::setGraphColor(QColor line, QColor fill)
{
    this->m_graph->setColor(line, fill);
}

// ── Paint ─────────────────────────────────────────────────────────────────────

void SidePanelItem::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);   // draw children (the graph widget)

    QPainter p(this);

    const QRect r = this->rect();

    // Background
    QColor bg;
    if (this->m_selected)
        bg = QColor(0x1a, 0x4a, 0x8a, 200);
    else if (this->m_hovered)
        bg = QColor(0x30, 0x30, 0x40, 120);
    else
        bg = QColor(0x1a, 0x1a, 0x22, 180);

    p.fillRect(r, bg);

    // Title (top-left, bold, slightly brighter when selected)
    QFont titleFont = this->font();
    titleFont.setBold(true);
    titleFont.setPointSize(8);
    p.setFont(titleFont);
    p.setPen(this->m_selected ? Qt::white : QColor(0xcc, 0xcc, 0xcc));
    p.drawText(QRect(6, 4, r.width() - 8, 16),
               Qt::AlignLeft | Qt::AlignVCenter,
               this->m_title);

    // Subtitle (top-right, small, muted)
    QFont subFont = this->font();
    subFont.setPointSize(7);
    p.setFont(subFont);
    p.setPen(QColor(0x88, 0xaa, 0xcc));
    p.drawText(QRect(6, 4, r.width() - 12, 16),
               Qt::AlignRight | Qt::AlignVCenter,
               this->m_subtitle);

    // Selection border
    if (this->m_selected)
    {
        p.setPen(QPen(QColor(0x44, 0x88, 0xff), 2));
        p.setBrush(Qt::NoBrush);
        p.drawRect(r.adjusted(1, 1, -1, -1));
    }
}

// ── Events ────────────────────────────────────────────────────────────────────

void SidePanelItem::mousePressEvent(QMouseEvent *event)
{
    QWidget::mousePressEvent(event);
    emit this->clicked();
}

void SidePanelItem::enterEvent(QEnterEvent *event)
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

} // namespace Perf
