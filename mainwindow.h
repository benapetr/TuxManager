#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QCloseEvent>
#include "processeswidget.h"
#include "performancewidget.h"
#include "userswidget.h"
#include "serviceswidget.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    Ui::MainWindow *ui;

    ProcessesWidget   *m_processesWidget;
    PerformanceWidget *m_performanceWidget;
    UsersWidget       *m_usersWidget;
    ServicesWidget    *m_servicesWidget;
};
#endif // MAINWINDOW_H
