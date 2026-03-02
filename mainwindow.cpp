#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "configuration.h"

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_processesWidget(new ProcessesWidget(this))
    , m_performanceWidget(new PerformanceWidget(this))
    , m_usersWidget(new UsersWidget(this))
    , m_servicesWidget(new ServicesWidget(this))
{
    this->ui->setupUi(this);

    this->ui->processesLayout->addWidget(this->m_processesWidget);
    this->ui->performanceLayout->addWidget(this->m_performanceWidget);
    this->ui->usersLayout->addWidget(this->m_usersWidget);
    this->ui->servicesLayout->addWidget(this->m_servicesWidget);

    // Restore previous window layout
    if (!CFG->WindowGeometry.isEmpty())
        this->restoreGeometry(CFG->WindowGeometry);
    if (!CFG->WindowState.isEmpty())
        this->restoreState(CFG->WindowState);
    this->ui->tabWidget->setCurrentIndex(CFG->ActiveTab);

    // Keep ActiveTab in sync as the user switches tabs
    connect(this->ui->tabWidget, &QTabWidget::currentChanged, this, [](int index) { CFG->ActiveTab = index; });
}

MainWindow::~MainWindow()
{
    delete this->ui;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    CFG->WindowGeometry = this->saveGeometry();
    CFG->WindowState    = this->saveState();
    CFG->Save();
    QMainWindow::closeEvent(event);
}
