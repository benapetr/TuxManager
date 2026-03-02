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
    ui->setupUi(this);

    ui->processesLayout->addWidget(m_processesWidget);
    ui->performanceLayout->addWidget(m_performanceWidget);
    ui->usersLayout->addWidget(m_usersWidget);
    ui->servicesLayout->addWidget(m_servicesWidget);

    // Restore previous window layout
    if (!CFG->WindowGeometry.isEmpty())
        restoreGeometry(CFG->WindowGeometry);
    if (!CFG->WindowState.isEmpty())
        restoreState(CFG->WindowState);
    ui->tabWidget->setCurrentIndex(CFG->ActiveTab);

    // Keep ActiveTab in sync as the user switches tabs
    connect(ui->tabWidget, &QTabWidget::currentChanged,
            this, [](int index) { CFG->ActiveTab = index; });
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    CFG->WindowGeometry = saveGeometry();
    CFG->WindowState    = saveState();
    CFG->Save();
    QMainWindow::closeEvent(event);
}
