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

#ifndef PERF_CPUGRAPHAREA_H
#define PERF_CPUGRAPHAREA_H

#include <QWidget>

class QGridLayout;
class QStackedWidget;

namespace Perf
{
    class GraphWidget;
    class PerfDataProvider;

    /// Switchable CPU graph container.
    ///
    /// Shows either a single aggregate utilisation graph (Overall)
    /// or a grid of per-core graphs (PerCore).
    ///
    /// Right-clicking the widget emits contextMenuRequested() so the parent
    /// (CpuDetailWidget) can build and show the context menu and call setMode() /
    /// setShowKernelTime() in response.
    class CpuGraphArea : public QWidget
    {
        Q_OBJECT

        public:
            enum class GraphMode { Overall, PerCore };

            explicit CpuGraphArea(QWidget *parent = nullptr);
            void SetProvider(const PerfDataProvider *provider);

            void SetMode(GraphMode mode);
            GraphMode GetMode() const { return this->m_mode; }

            void SetShowKernelTime(bool show);
            bool GetShowKernelTime() const { return this->m_showKernelTime; }
            void ApplyColorScheme();

            /// Call after every PerfDataProvider::updated() signal.
            void UpdateData();

        signals:
            void contextMenuRequested(const QPoint &globalPos);

        protected:
            void contextMenuEvent(QContextMenuEvent *event) override;

        private:
            void ensureCoreGraphs(int count);
            void bindOverallGraphSources();
            void bindCoreGraphSources(int count);

            QStackedWidget        *m_stack           { nullptr };
            GraphWidget           *m_overallGraph     { nullptr };
            QWidget               *m_perCoreContainer { nullptr };
            QGridLayout           *m_perCoreGrid      { nullptr };
            QVector<GraphWidget *> m_coreGraphs;
            const PerfDataProvider *m_provider        { nullptr };

            GraphMode              m_mode            { GraphMode::Overall };
            bool                   m_showKernelTime  { false };
    };
} // namespace Perf

#endif // PERF_CPUGRAPHAREA_H
