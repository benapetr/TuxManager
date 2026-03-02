#ifndef PERF_GRAPHWIDGET_H
#define PERF_GRAPHWIDGET_H

#include <QColor>
#include <QVector>
#include <QWidget>

namespace Perf
{

/// Reusable scrolling-graph widget used both in the side panel mini-thumbnails
/// and in the detail panes.
///
/// Call setHistory() to push a new data snapshot, then the widget repaints.
/// Values are assumed to be in the range [0, maxVal].
class GraphWidget : public QWidget
{
    Q_OBJECT

    public:
        explicit GraphWidget(QWidget *parent = nullptr);

        /// Replace the displayed history and trigger a repaint.
        void setHistory(const QVector<double> &data, double maxVal = 100.0);

        /// Optional secondary (kernel-time) history drawn as a darker overlay.
        /// Pass an empty vector to disable.
        void setSecondaryHistory(const QVector<double> &data2);

        /// Optional: change the line / fill colour pair from the default blue.
        void setColor(QColor line, QColor fill);

        /// Number of horizontal grid divisions.
        void setGridColumns(int cols) { this->m_gridCols = cols; update(); }
        /// Number of vertical grid divisions.
        void setGridRows(int rows)    { this->m_gridRows = rows; update(); }

        QSize sizeHint() const override { return QSize(200, 80); }
        QSize minimumSizeHint() const override { return QSize(60, 30); }

    protected:
        void paintEvent(QPaintEvent *event) override;

    private:
        QVector<double> m_data;
        QVector<double> m_data2;            ///< kernel-time overlay (optional)
        double          m_maxVal    { 100.0 };

        QColor          m_lineColor  { 0x00, 0xbc, 0xff };  // bright blue
        QColor          m_fillColor  { 0x00, 0x4c, 0x8a, 120 };
        QColor          m_fillColor2 { 0x00, 0x22, 0x55, 160 };  // darker blue for kernel

        int             m_gridCols  { 5 };
        int             m_gridRows  { 4 };
};

} // namespace Perf

#endif // PERF_GRAPHWIDGET_H
