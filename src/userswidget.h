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

#ifndef USERSWIDGET_H
#define USERSWIDGET_H

#include "os/process.h"
#include "os/processrefreshservice.h"

#include <QHash>
#include <Qt>
#include <QTimer>
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
        explicit UsersWidget(OS::ProcessRefreshService *processRefreshService, QWidget *parent = nullptr);
        ~UsersWidget();
        void SetActive(bool active);
        bool IsActive() const { return this->m_active; }

    signals:
        void goToProcessRequested(pid_t pid);

    private slots:
        void onTimerTick();
        void onRefreshFinished(int consumer, quint64 token, const QList<OS::Process> &processes);
        void onContextMenu(const QPoint &pos);

    private:
        Ui::UsersWidget *ui;
        OS::ProcessRefreshService *m_processRefreshService { nullptr };
        QTimer          *m_refreshTimer { nullptr };
        bool             m_active { false };
        bool             m_refreshInFlight { false };
        bool             m_refreshPending { false };
        quint64          m_refreshToken { 0 };
        int              m_sortColumn { 1 };
        Qt::SortOrder    m_sortOrder { Qt::DescendingOrder };

        void startRefresh();
        void rebuildTree(const QList<OS::Process> &allProcs);
};

#endif // USERSWIDGET_H
