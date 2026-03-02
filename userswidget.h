#ifndef USERSWIDGET_H
#define USERSWIDGET_H

#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui
{
    class UsersWidget;
}
QT_END_NAMESPACE

class UsersWidget : public QWidget
{
    Q_OBJECT

    public:
        explicit UsersWidget(QWidget *parent = nullptr);
        ~UsersWidget();

    private:
        Ui::UsersWidget *ui;
};

#endif // USERSWIDGET_H
