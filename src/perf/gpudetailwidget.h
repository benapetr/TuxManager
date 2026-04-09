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

#ifndef PERF_GPUDETAILWIDGET_H
#define PERF_GPUDETAILWIDGET_H

#include "graphwidget.h"
#include "historybuffer.h"
#include "perfdataprovider.h"

#include <QComboBox>
#include <QLabel>
#include <QVector>
#include <QWidget>

namespace Perf
{
    class GpuDetailWidget : public QWidget
    {
        Q_OBJECT

        public:
            explicit GpuDetailWidget(QWidget *parent = nullptr);

            void SetGpu(PerfDataProvider *provider, int index);
            void ApplyColorScheme();

        private slots:
            void onUpdated();
            void onEngineSelectionChanged(int slot, int comboIndex);

        private:
            void rebuildEngineSelectors();
            void bindGpuIdentity();
            void bindEngineGraphSource(int slot);
            void bindMemoryAndCopySources(bool hasSharedData);

            PerfDataProvider *m_provider { nullptr };
            int               m_gpuIndex { -1 };

            QLabel *m_titleLabel { nullptr };
            QLabel *m_modelLabel { nullptr };
            QLabel *m_utilValueLabel { nullptr };
            QLabel *m_tempValueLabel { nullptr };
            QLabel *m_gpuMemValueLabel { nullptr };
            QLabel *m_dedicatedMemValueLabel { nullptr };
            QLabel *m_sharedMemValueLabel { nullptr };
            QLabel *m_driverValueLabel { nullptr };
            QLabel *m_backendValueLabel { nullptr };
            QLabel *m_dedicatedMemGraphMaxLabel { nullptr };
            QLabel *m_sharedMemGraphMaxLabel { nullptr };
            QLabel *m_copyBwGraphMaxLabel { nullptr };
            QLabel *m_copyBwLegendLabel { nullptr };

            QVector<QComboBox *>   m_engineSelectors;
            QVector<QLabel *>      m_engineValueLabels;
            QVector<GraphWidget *> m_engineGraphs;
            QVector<int>           m_selectedEngineBySlot;

            GraphWidget *m_dedicatedMemGraph { nullptr };
            GraphWidget *m_sharedMemGraph { nullptr };
            GraphWidget *m_copyBwGraph { nullptr };
            const HistoryBuffer *m_dedicatedMemHistory { nullptr };
            const HistoryBuffer *m_sharedMemHistorySource { nullptr };
            const HistoryBuffer *m_copyTxHistory { nullptr };
            const HistoryBuffer *m_copyRxHistory { nullptr };
            HistoryBuffer m_sharedMemHistory { HISTORY_SIZE };
    };
} // namespace Perf

#endif // PERF_GPUDETAILWIDGET_H
