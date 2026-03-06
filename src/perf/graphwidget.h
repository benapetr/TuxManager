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
    /// Call SetHistory() to push a new data snapshot, then the widget repaints.
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
            void SetHistory(const QVector<double> &data, double maxVal = 100.0);

            /// Optional secondary (kernel-time) history drawn as a darker overlay.
            /// Pass an empty vector to disable.
            void SetSecondaryHistory(const QVector<double> &data2);
            const QVector<double> &secondaryHistory() const { return this->m_data2; }

            /// Optional: change the line / fill colour pair from the default blue.
            void SetColor(QColor line, QColor fill);

            /// Number of horizontal grid divisions.
            void SetGridColumns(int cols) { this->m_gridCols = cols; update(); }
            /// Number of vertical grid divisions.
            void SetGridRows(int rows)    { this->m_gridRows = rows; update(); }
            /// Number of fixed time slots across the X axis (controls scrolling).
            void SetSampleCapacity(int samples);

            void SetHoverLineEnabled(bool enabled) { this->m_hoverLineEnabled = enabled; update(); }
            void SetHoverTooltipEnabled(bool enabled) { this->m_hoverTooltipEnabled = enabled; }
            void SetSeriesNames(const QString &primary, const QString &secondary = QString());
            void SetValueFormat(ValueFormat fmt) { this->m_valueFormat = fmt; }
            void SetPercentTooltipAbsolute(double maxAbsoluteValue,
                                           const QString &unitLabel,
                                           int precision = 2);

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
            bool            m_percentTooltipAbsoluteEnabled { false };
            double          m_percentTooltipAbsoluteMax { 0.0 };
            QString         m_percentTooltipAbsoluteUnit;
            int             m_percentTooltipAbsolutePrecision { 2 };

            static int sampleIndexForSlot(int size, int slot, int sampleCount);
            QString formatValue(double v) const;
    };
} // namespace Perf

#endif // PERF_GRAPHWIDGET_H
