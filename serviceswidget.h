#ifndef SERVICESWIDGET_H
#define SERVICESWIDGET_H

#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui {
class ServicesWidget;
}
QT_END_NAMESPACE

class ServicesWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ServicesWidget(QWidget *parent = nullptr);
    ~ServicesWidget();

private:
    Ui::ServicesWidget *ui;
};

#endif // SERVICESWIDGET_H
