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
        explicit UsersWidget(QWidget *parent = nullptr);
        ~UsersWidget();
        void SetActive(bool active);
        bool IsActive() const { return this->m_active; }

    signals:
        void goToProcessRequested(pid_t pid);

    private slots:
        void onTimerTick();
        void onContextMenu(const QPoint &pos);

    private:
        Ui::UsersWidget *ui;
        QTimer          *m_refreshTimer { nullptr };
        bool             m_active { false };
        QHash<pid_t, quint64> m_prevTicks;
        quint64          m_prevCpuTotalTicks { 0 };
        int              m_numCpus { 1 };
        int              m_sortColumn { 1 };
        Qt::SortOrder    m_sortOrder { Qt::DescendingOrder };

        void rebuildTree(const QList<OS::Process> &allProcs);
};

#endif // USERSWIDGET_H
