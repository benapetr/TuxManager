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

#ifndef PERF_SWAPDETAILWIDGET_H
#define PERF_SWAPDETAILWIDGET_H

#include "historybuffer.h"
#include "perfdataprovider.h"
#include "graphwidget.h"

#include <QLabel>
#include <QVector>
#include <QWidget>

namespace Perf
{
    class SwapDetailWidget : public QWidget
    {
        Q_OBJECT

        public:
            explicit SwapDetailWidget(QWidget *parent = nullptr);

            void SetProvider(PerfDataProvider *provider);
            void ApplyColorScheme();

        private slots:
            void onUpdated();

        private:
            PerfDataProvider *m_provider { nullptr };

            QLabel *m_titleLabel { nullptr };
            QLabel *m_totalLabel { nullptr };
            GraphWidget *m_usageGraph { nullptr };
            QLabel *m_usageValueLabel { nullptr };

            GraphWidget *m_activityGraph { nullptr };
            QLabel *m_activityMaxLabel { nullptr };

            QLabel *m_inUseValueLabel { nullptr };
            QLabel *m_freeValueLabel { nullptr };
            QLabel *m_inRateValueLabel { nullptr };
            QLabel *m_outRateValueLabel { nullptr };
            QVector<QLabel *> m_statLabels;
            QVector<QLabel *> m_axisLabels;

            const HistoryBuffer *m_usageHistory { nullptr };
            const HistoryBuffer *m_inHistory { nullptr };
            const HistoryBuffer *m_outHistory { nullptr };
    };
} // namespace Perf

#endif // PERF_SWAPDETAILWIDGET_H
