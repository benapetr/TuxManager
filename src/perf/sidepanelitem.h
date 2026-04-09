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

#ifndef PERF_SIDEPANELITEM_H
#define PERF_SIDEPANELITEM_H

#include "graphwidget.h"
#include "historybuffer.h"

#include <QPoint>
#include <QVector>
#include <QWidget>
#include <QtGlobal>

namespace Perf
{
    /// One entry in the left-hand side panel of the Performance tab.
    /// Displays a title, a subtitle (e.g. "3% 3.40 GHz"), and an embedded
    /// mini sparkline graph.  Emits clicked() when the user presses it.
    class SidePanelItem : public QWidget
    {
        Q_OBJECT

        public:
            explicit SidePanelItem(const QString &title, QWidget *parent = nullptr);

            void SetGraphSource(const HistoryBuffer &history, double maxVal = 100.0);
            void Update(const QString &subtitle, double maxVal = 100.0);

            void SetSelected(bool selected);
            bool IsSelected() const { return this->m_selected; }

            void SetGraphColor(QColor line, QColor fill);

            QSize sizeHint()        const override { return QSize(160, 80); }
            QSize minimumSizeHint() const override { return QSize(120, 70); }

        signals:
            void clicked();
            void contextMenuRequested(const QPoint &globalPos);

        protected:
            void paintEvent(QPaintEvent *event) override;
            void mousePressEvent(QMouseEvent *event) override;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            void enterEvent(QEnterEvent *event) override;
#else
            void enterEvent(QEvent *event) override;
#endif
            void leaveEvent(QEvent *event) override;

        private:
            QString      m_title;
            QString      m_subtitle;
            GraphWidget *m_graph;
            bool         m_selected { false };
            bool         m_hovered  { false };
    };
} // namespace Perf

#endif // PERF_SIDEPANELITEM_H
