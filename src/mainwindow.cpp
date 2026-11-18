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
#include "aboutdialog.h"
#include "ui/thememanager.h"
#include "ui/uihelper.h"

#include <QActionGroup>
#include <QMenuBar>
#include <QMenu>

#include <QGuiApplication>
#include <QScreen>
#include <QtGlobal>

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
        this->setWindowTitle(this->windowTitle() + tr(" (superuser)"));
    }

    this->ui->processesLayout->addWidget(this->m_processesWidget);
    this->ui->performanceLayout->addWidget(this->m_performanceWidget);
    this->ui->usersLayout->addWidget(this->m_usersWidget);
    this->ui->servicesLayout->addWidget(this->m_servicesWidget);

    this->setupSettingsMenu();
    connect(ThemeManager::Get(), &ThemeManager::Changed,
            this->m_performanceWidget, &PerformanceWidget::ApplyColorScheme);

    // Restore previous window layout
    bool geometryRestored = false;
    if (!CFG->WindowGeometry.isEmpty())
        geometryRestored = this->restoreGeometry(CFG->WindowGeometry);
    if (!CFG->WindowState.isEmpty())
        this->restoreState(CFG->WindowState);

    // A saved window may have been larger than the current display. Keep both
    // saved and default geometry within the available screen area.
    if (!this->isMaximized() && !this->isFullScreen())
    {
        QScreen *screen = geometryRestored
            ? QGuiApplication::screenAt(this->frameGeometry().center())
            : nullptr;
        if (!screen)
            screen = QGuiApplication::primaryScreen();

        if (screen)
        {
            const QRect available = screen->availableGeometry();
            const QSize maxSize(qMax(1, available.width() * 9 / 10),
                                qMax(1, available.height() * 9 / 10));
            const QSize boundedSize = this->size().boundedTo(maxSize);
            const bool sizeChanged = boundedSize != this->size();
            if (sizeChanged)
                this->resize(boundedSize);

            if (!geometryRestored || sizeChanged || !available.contains(this->frameGeometry()))
            {
                this->move(available.left() + (available.width() - this->width()) / 2,
                           available.top() + (available.height() - this->height()) / 2);
            }
        }
    }

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

void MainWindow::setupSettingsMenu()
{
    QMenu *settings = this->menuBar()->addMenu(tr("&Settings"));
    settings->setObjectName("settingsMenu");
    QMenu *appearance = settings->addMenu(tr("&Appearance"));
    auto *group = new QActionGroup(appearance);
    const QStringList modes { "system", "light", "dark" };
    const QStringList labels { tr("Follow system"), tr("Light"), tr("Dark") };
    for (int i = 0; i < modes.size(); ++i)
    {
        QAction *action = appearance->addAction(labels.at(i));
        action->setObjectName("appearance_" + modes.at(i));
        action->setData(modes.at(i));
        action->setCheckable(true);
        action->setChecked(CFG->AppearanceMode == modes.at(i));
        group->addAction(action);
    }
    connect(group, &QActionGroup::triggered, this, [](QAction *action)
    {
        ThemeManager::Get()->SetMode(action->data().toString());
    });
    connect(appearance, &QMenu::aboutToShow, this, [group]()
    {
        for (QAction *action : group->actions())
            action->setChecked(action->data().toString() == CFG->AppearanceMode);
    });
    settings->addAction(tr("Customize graph colors..."), this->m_performanceWidget, &PerformanceWidget::CustomizeColors);
    settings->addSeparator();
    UIHelper::AddRefreshIntervalContextMenu(settings);
    UIHelper::AddGraphWindowContextMenu(settings);

    QMenu *sidebar = settings->addMenu(tr("Performance sidebar"));
    QAction *grid = sidebar->addAction(tr("Show grid"));
    grid->setCheckable(true);
    grid->setChecked(CFG->SidePanelGridEnabled);
    connect(grid, &QAction::triggered, this->m_performanceWidget, &PerformanceWidget::SetSidePanelGridEnabled);
    connect(sidebar, &QMenu::aboutToShow, this, [grid]() { grid->setChecked(CFG->SidePanelGridEnabled); });
    sidebar->addAction(tr("Customize order..."), this->m_performanceWidget, &PerformanceWidget::CustomizeOrder);

    QMenu *help = this->menuBar()->addMenu(tr("&Help"));
    help->addAction(tr("About TuxManager"), this, [this]()
    {
        AboutDialog dialog(this);
        dialog.exec();
    });
}
