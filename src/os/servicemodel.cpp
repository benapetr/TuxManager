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

#include "servicemodel.h"

using namespace OS;

namespace
{
    QString displayServiceUnit(const QString &unit)
    {
        if (unit.endsWith(".service"))
            return unit.left(unit.size() - 8);
        return unit;
    }
}

ServiceModel::ServiceModel(QObject *parent) : QAbstractTableModel(parent)
{
}

int ServiceModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return this->m_services.size();
}

int ServiceModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return ColCount;
}

QVariant ServiceModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()
        || index.row() < 0
        || index.row() >= this->m_services.size()
        || index.column() < 0
        || index.column() >= ColCount)
    {
        return {};
    }

    const Service &s = this->m_services.at(index.row());

    if (role == Qt::DisplayRole)
    {
        switch (static_cast<Column>(index.column()))
        {
            case ColService:     return displayServiceUnit(s.Unit);
            case ColLoad:        return s.LoadState;
            case ColActive:      return s.ActiveState;
            case ColSubState:    return s.SubState;
            case ColDescription: return s.Description;
            default:             return {};
        }
    }

    if (role == Qt::UserRole)
    {
        switch (static_cast<Column>(index.column()))
        {
            case ColService:     return s.Unit;
            case ColLoad:        return s.LoadState;
            case ColActive:      return s.ActiveState;
            case ColSubState:    return s.SubState;
            case ColDescription: return s.Description;
            default:             return {};
        }
    }

    return {};
}

QVariant ServiceModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};
    if (section < 0 || section >= ColCount)
        return {};

    switch (static_cast<Column>(section))
    {
        case ColService:     return tr("服务");
        case ColLoad:        return tr("加载");
        case ColActive:      return tr("活动");
        case ColSubState:    return tr("子状态");
        case ColDescription: return tr("描述");
        default:             return {};
    }
}

Qt::ItemFlags ServiceModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

void ServiceModel::SetServices(const QList<Service> &services)
{
    beginResetModel();
    this->m_services = services;
    endResetModel();
}
