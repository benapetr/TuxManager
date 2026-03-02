#ifndef PERF_CPUDETAILWIDGET_H
#define PERF_CPUDETAILWIDGET_H

#include "cpugrapharea.h"
#include "perfdataprovider.h"

#include <QMenu>
#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui { class CpuDetailWidget; }
QT_END_NAMESPACE

namespace Perf
{

class CpuDetailWidget : public QWidget
{
    Q_OBJECT

    public:
        explicit CpuDetailWidget(QWidget *parent = nullptr);
        ~CpuDetailWidget();

        void setProvider(PerfDataProvider *provider);

    private slots:
        void onUpdated();
        void onContextMenuRequested(const QPoint &globalPos);

    private:
        Ui::CpuDetailWidget  *ui;
        PerfDataProvider     *m_provider   { nullptr };
        CpuGraphArea         *m_graphArea  { nullptr };
};

} // namespace Perf

#endif // PERF_CPUDETAILWIDGET_H

