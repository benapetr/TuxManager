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

        void setMode(GraphMode mode);
        GraphMode mode() const { return this->m_mode; }

        void setShowKernelTime(bool show);
        bool showKernelTime() const { return this->m_showKernelTime; }

        /// Call after every PerfDataProvider::updated() signal.
        void updateData(const PerfDataProvider *provider);

    signals:
        void contextMenuRequested(const QPoint &globalPos);

    protected:
        void contextMenuEvent(QContextMenuEvent *event) override;

    private:
        void ensureCoreGraphs(int count);

        QStackedWidget        *m_stack           { nullptr };
        GraphWidget           *m_overallGraph     { nullptr };
        QWidget               *m_perCoreContainer { nullptr };
        QGridLayout           *m_perCoreGrid      { nullptr };
        QVector<GraphWidget *> m_coreGraphs;

        GraphMode              m_mode            { GraphMode::Overall };
        bool                   m_showKernelTime  { false };
};

} // namespace Perf

#endif // PERF_CPUGRAPHAREA_H
