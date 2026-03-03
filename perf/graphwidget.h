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
        enum class ValueFormat
        {
            Auto,          ///< Percent if max<=100, otherwise raw number.
            Percent,       ///< 0..100%
            BytesPerSec,   ///< B/s, KB/s, MB/s, GB/s
            Raw            ///< Plain number
        };

        explicit GraphWidget(QWidget *parent = nullptr);

        /// Replace the displayed history and trigger a repaint.
        void setHistory(const QVector<double> &data, double maxVal = 100.0);

        /// Optional secondary (kernel-time) history drawn as a darker overlay.
        /// Pass an empty vector to disable.
        void setSecondaryHistory(const QVector<double> &data2);
        const QVector<double> &secondaryHistory() const { return this->m_data2; }

        /// Optional: change the line / fill colour pair from the default blue.
        void setColor(QColor line, QColor fill);

        /// Number of horizontal grid divisions.
        void setGridColumns(int cols) { this->m_gridCols = cols; update(); }
        /// Number of vertical grid divisions.
        void setGridRows(int rows)    { this->m_gridRows = rows; update(); }
        /// Number of fixed time slots across the X axis (controls scrolling).
        void setSampleCapacity(int samples);

        void setHoverLineEnabled(bool enabled) { this->m_hoverLineEnabled = enabled; update(); }
        void setHoverTooltipEnabled(bool enabled) { this->m_hoverTooltipEnabled = enabled; }
        void setSeriesNames(const QString &primary, const QString &secondary = QString());
        void setValueFormat(ValueFormat fmt) { this->m_valueFormat = fmt; }

        QSize sizeHint() const override { return QSize(200, 80); }
        QSize minimumSizeHint() const override { return QSize(60, 30); }

    protected:
        void paintEvent(QPaintEvent *event) override;
        void mouseMoveEvent(QMouseEvent *event) override;
        void leaveEvent(QEvent *event) override;

    private:
        QVector<double> m_data;
        QVector<double> m_data2;            ///< kernel-time overlay (optional)
        double          m_maxVal    { 100.0 };

        QColor          m_lineColor  { 0x00, 0xbc, 0xff };  // bright blue
        QColor          m_fillColor  { 0x00, 0x4c, 0x8a, 120 };
        QColor          m_fillColor2 { 0x00, 0x22, 0x55, 160 };  // darker blue for kernel

        int             m_gridCols  { 5 };
        int             m_gridRows  { 4 };
        int             m_sampleCapacity { 60 };  ///< Matches PerfDataProvider::HISTORY_SIZE.
        int             m_historyTick { 0 };      ///< Advances as samples arrive; used for grid phase.
        int             m_hoverSlot { -1 };
        bool            m_hoverLineEnabled { true };
        bool            m_hoverTooltipEnabled { true };
        QString         m_primaryName { tr("Value") };
        QString         m_secondaryName { tr("Secondary") };
        ValueFormat     m_valueFormat { ValueFormat::Auto };

        static int sampleIndexForSlot(int size, int slot, int sampleCount);
        QString formatValue(double v) const;
};

} // namespace Perf

#endif // PERF_GRAPHWIDGET_H
