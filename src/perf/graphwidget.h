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

#include "historybuffer.h"

#include <QColor>
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
                BitsPerSec,    ///< b/s, Kb/s, Mb/s, Gb/s
                Raw            ///< Plain number
            };

            explicit GraphWidget(QWidget *parent = nullptr);

            //! Pointer to a vector containing values to draw graph data from
            void SetDataSource(const HistoryBuffer &data, double maxVal = 100.0);

            //! Optional secondary graph drawn as a darker overlay.
            void SetOverlayDataSource(const HistoryBuffer &data);

            void SetMax(double maxVal = 100.0);

            //! Push graph timeline on X axis into future.
            //! Hidden graphs can advance without scheduling a repaint.
            void Tick(bool requestUpdate = true);

            void ClearDataSource();
            void ClearOverlayDataSource();

            //! Optional: change the line / fill colour pair from the default blue.
            void SetColor(QColor line, QColor fill, QColor fill2 = QColor());

            //! Number of horizontal grid divisions.
            void SetGridColumns(int cols) { this->m_gridCols = cols; update(); }
            //! Number of vertical grid divisions.
            void SetGridRows(int rows)    { this->m_gridRows = rows; update(); }
            //! Number of fixed time slots across the X axis (controls scrolling).
            void SetSampleCapacity(int samples);

            //! Enable/disable the vertical guide line that follows mouse hover.
            void SetHoverLineEnabled(bool enabled) { this->m_hoverLineEnabled = enabled; update(); }
            //! Enable/disable value tooltips shown while hovering the graph.
            void SetHoverTooltipEnabled(bool enabled) { this->m_hoverTooltipEnabled = enabled; }
            //! Set display names used in hover tooltip for primary/secondary series.
            void SetSeriesNames(const QString &primary, const QString &secondary = QString());
            //! Select value formatting mode for tooltip text rendering.
            void SetValueFormat(ValueFormat fmt) { this->m_valueFormat = fmt; }
            //! Draw a fixed helper label in the graph area (for per-core frequency etc.).
            void SetOverlayText(const QString &text);
            //! For percent histories, also show absolute value in tooltips using max*percent.
            void SetPercentTooltipAbsolute(double maxAbsoluteValue, const QString &unitLabel, int precision = 2);

            QSize sizeHint() const override { return QSize(200, 80); }
            QSize minimumSizeHint() const override { return QSize(60, 30); }

        protected:
            void paintEvent(QPaintEvent *event) override;
            void mouseMoveEvent(QMouseEvent *event) override;
            void leaveEvent(QEvent *event) override;

        private:
            const HistoryBuffer *m_data { nullptr };
            const HistoryBuffer *m_overlayData { nullptr }; ///< overlay (optional)
            double          m_maxVal    { 100.0 };

            QColor          m_lineColor;
            QColor          m_fillColor;
            QColor          m_fillColor2;

            int             m_gridCols  { 5 };
            int             m_gridRows  { 4 };
            int             m_sampleCapacity { 60 };  ///< Matches the default history window in seconds.
            int             m_historyTick { 0 };      ///< Advances as samples arrive; used for grid phase.
            int             m_hoverSlot { -1 };
            bool            m_hoverLineEnabled { true };
            bool            m_hoverTooltipEnabled { true };
            QString         m_primaryName { tr("Value") };
            QString         m_secondaryName { tr("Secondary") };
            ValueFormat     m_valueFormat { ValueFormat::Auto };
            QString         m_overlayText;
            bool            m_percentTooltipAbsoluteEnabled { false };
            double          m_percentTooltipAbsoluteMax { 0.0 };
            QString         m_percentTooltipAbsoluteUnit;
            int             m_percentTooltipAbsolutePrecision { 2 };

            QRect hoverLineRect(int slot) const;
            static int sampleIndexForSlot(int size, int slot, int sampleCount);
            QString formatValue(double v) const;
    };
} // namespace Perf

#endif // PERF_GRAPHWIDGET_H
