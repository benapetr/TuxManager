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

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "configuration.h"

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_processRefreshService(new OS::ProcessRefreshService(this))
    , m_processesWidget(new ProcessesWidget(this->m_processRefreshService, this))
    , m_performanceWidget(new PerformanceWidget(this))
    , m_usersWidget(new UsersWidget(this->m_processRefreshService, this))
    , m_servicesWidget(new ServicesWidget(this))
{
    this->ui->setupUi(this);

    if (CFG->IsSuperuser)
    {
        // Make user aware of this
        this->setWindowTitle(this->windowTitle() + " (superuser)");
    }

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
    this->updateTabActivity(this->ui->tabWidget->currentIndex());

    // Keep ActiveTab in sync as the user switches tabs
    connect(this->ui->tabWidget, &QTabWidget::currentChanged, this, [this](int index)
    {
        CFG->ActiveTab = index;
        this->updateTabActivity(index);
    });

    connect(this->m_usersWidget, &UsersWidget::goToProcessRequested, this, [this](pid_t pid)
    {
        this->ui->tabWidget->setCurrentIndex(0);
        this->m_processesWidget->ClearSearchFilter();
        this->m_processesWidget->SelectProcessByPid(pid);
    });
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

void MainWindow::updateTabActivity(int index)
{
    // Tabs: 0=Processes, 1=Performance, 2=Users, 3=Services
    this->m_processesWidget->SetActive(index == 0);
    this->m_performanceWidget->SetActive(index == 1);
    this->m_usersWidget->SetActive(index == 2);
    this->m_servicesWidget->SetActive(index == 3);
}
