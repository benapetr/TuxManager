#ifndef PERFORMANCEWIDGET_H
#define PERFORMANCEWIDGET_H

#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui
{
    class PerformanceWidget;
}
QT_END_NAMESPACE

class PerformanceWidget : public QWidget
{
    Q_OBJECT

    public:
        explicit PerformanceWidget(QWidget *parent = nullptr);
        ~PerformanceWidget();

    private:
        Ui::PerformanceWidget *ui;
};

#endif // PERFORMANCEWIDGET_H
