#ifndef PROCESSESWIDGET_H
#define PROCESSESWIDGET_H

#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui {
class ProcessesWidget;
}
QT_END_NAMESPACE

class ProcessesWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ProcessesWidget(QWidget *parent = nullptr);
    ~ProcessesWidget();

private:
    Ui::ProcessesWidget *ui;
};

#endif // PROCESSESWIDGET_H
