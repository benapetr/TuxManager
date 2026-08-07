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

#include "processtreemodel.h"
#include "../misc.h"
using namespace OS;

ProcessTreeModel::ProcessTreeModel(QObject *parent) : QAbstractItemModel(parent), m_root(new Node())
{
}

ProcessTreeModel::~ProcessTreeModel()
{
    freeNode(this->m_root);
}

QModelIndex ProcessTreeModel::index(int row, int column, const QModelIndex &parent) const
{
    if (row < 0 || column < 0 || column >= ColCount)
        return {};

    Node *parentNode = nodeFromIndex(parent);
    if (!parentNode)
        parentNode = this->m_root;
    if (row >= parentNode->children.size())
        return {};

    return createIndex(row, column, parentNode->children.at(row));
}

QModelIndex ProcessTreeModel::parent(const QModelIndex &child) const
{
    if (!child.isValid())
        return {};

    Node *node = nodeFromIndex(child);
    if (!node || !node->parent || node->parent == this->m_root)
        return {};

    Node *parentNode = node->parent;
    Node *grandParent = parentNode->parent ? parentNode->parent : this->m_root;
    const int row = grandParent->children.indexOf(parentNode);
    if (row < 0)
        return {};
    return createIndex(row, 0, parentNode);
}

int ProcessTreeModel::rowCount(const QModelIndex &parent) const
{
    Node *parentNode = nodeFromIndex(parent);
    if (!parentNode)
        parentNode = this->m_root;
    return parentNode->children.size();
}

int ProcessTreeModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return ColCount;
}

QVariant ProcessTreeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.column() < 0 || index.column() >= ColCount)
        return {};

    Node *node = nodeFromIndex(index);
    if (!node)
        return {};
    const Process &proc = node->process;

    if (role == Qt::DisplayRole)
    {
        switch (static_cast<Column>(index.column()))
        {
            case ColPid:      return proc.PID;
            case ColName:     return proc.Name;
            case ColUser:     return proc.User;
            case ColState:    return Process::GetStateString(proc.State);
            case ColCpu:      return QString::number(proc.CPUPercent, 'f', 1) + " %";
            case ColMemRss:   return Misc::FormatKiB(proc.VMRssKb, 0);
            case ColMemVirt:  return Misc::FormatKiB(proc.vmSizeKb, 0);
            case ColIoReads:  return proc.IOTotalsAvailable ? Misc::FormatBytes(proc.IOReadBytes, 0) : QString("?");
            case ColIoWrites: return proc.IOTotalsAvailable ? Misc::FormatBytes(proc.IOWriteBytes, 0) : QString("?");
            case ColIoReadsPerSec:
                if (proc.IOPermissionDenied)
                    return QString("?");
                return proc.IORatesAvailable ? Misc::FormatBytesPerSecond(proc.IOReadBps) : tr("测量中...");
            case ColIoWritesPerSec:
                if (proc.IOPermissionDenied)
                    return QString("?");
                return proc.IORatesAvailable ? Misc::FormatBytesPerSecond(proc.IOWriteBps) : tr("测量中...");
            case ColThreads:  return proc.Threads;
            case ColPriority: return proc.Priority;
            case ColNice:     return proc.Nice;
            case ColCmdline:  return proc.CmdLine;
            default: break;
        }
    }

    if (role == Qt::UserRole)
    {
        switch (static_cast<Column>(index.column()))
        {
            case ColPid:      return static_cast<qlonglong>(proc.PID);
            case ColCpu:      return proc.CPUPercent;
            case ColMemRss:   return static_cast<qulonglong>(proc.VMRssKb);
            case ColMemVirt:  return static_cast<qulonglong>(proc.vmSizeKb);
            case ColIoReads:
                return proc.IOTotalsAvailable
                       ? QVariant::fromValue(static_cast<qlonglong>(proc.IOReadBytes))
                       : QVariant::fromValue(static_cast<qlonglong>(-1));
            case ColIoWrites:
                return proc.IOTotalsAvailable
                       ? QVariant::fromValue(static_cast<qlonglong>(proc.IOWriteBytes))
                       : QVariant::fromValue(static_cast<qlonglong>(-1));
            case ColIoReadsPerSec:
                return proc.IORatesAvailable
                       ? QVariant::fromValue(proc.IOReadBps)
                       : QVariant::fromValue(-1.0);
            case ColIoWritesPerSec:
                return proc.IORatesAvailable
                       ? QVariant::fromValue(proc.IOWriteBps)
                       : QVariant::fromValue(-1.0);
            case ColThreads:  return proc.Threads;
            case ColPriority: return proc.Priority;
            case ColNice:     return proc.Nice;
            default:          return data(index, Qt::DisplayRole);
        }
    }

    if (role == Qt::TextAlignmentRole)
    {
        switch (static_cast<Column>(index.column()))
        {
            case ColCpu:
            case ColMemRss:
            case ColMemVirt:
            case ColIoReads:
            case ColIoWrites:
            case ColIoReadsPerSec:
            case ColIoWritesPerSec:
            case ColThreads:
            case ColPriority:
            case ColNice:
                return QVariant(Qt::AlignRight | Qt::AlignVCenter);
            default:
                return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
        }
    }

    return {};
}

QVariant ProcessTreeModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
        return {};
    if (section < 0 || section >= ColCount)
        return {};
    return columnHeader(static_cast<Column>(section));
}

Qt::ItemFlags ProcessTreeModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

void ProcessTreeModel::SetProcesses(const QList<Process> &processes)
{
    beginResetModel();
    freeNode(this->m_root);
    this->m_root = new Node();
    this->m_byPid.clear();

    QList<Node *> nodes;
    nodes.reserve(processes.size());
    for (const Process &proc : processes)
    {
        Node *node = new Node();
        node->process = proc;
        this->m_byPid.insert(proc.PID, node);
        nodes.append(node);
    }

    for (Node *node : nodes)
    {
        Node *parent = this->m_root;
        const pid_t ppid = node->process.PPID;
        if (ppid > 0 && ppid != node->process.PID && this->m_byPid.contains(ppid))
            parent = this->m_byPid.value(ppid);

        node->parent = parent;
        parent->children.append(node);
    }

    endResetModel();
}

QModelIndex ProcessTreeModel::IndexForPid(pid_t pid) const
{
    Node *node = this->m_byPid.value(pid, nullptr);
    if (!node || !node->parent)
        return {};
    const int row = node->parent->children.indexOf(node);
    if (row < 0)
        return {};
    return createIndex(row, 0, node);
}

QString ProcessTreeModel::columnHeader(Column col)
{
    switch (col)
    {
        case ColPid:      return "PID";
        case ColName:     return "名称";
        case ColUser:     return "用户";
        case ColState:    return "状态";
        case ColCpu:      return "CPU %";
        case ColMemRss:   return "常驻内存";
        case ColMemVirt:  return "虚拟内存";
        case ColIoReads:  return "IO 读取";
        case ColIoWrites: return "IO 写入";
        case ColIoReadsPerSec: return "IO 读取/秒";
        case ColIoWritesPerSec:return "IO 写入/秒";
        case ColThreads:  return "线程";
        case ColPriority: return "优先级";
        case ColNice:     return "Nice";
        case ColCmdline:  return "命令";
        default:          return {};
    }
}

void ProcessTreeModel::freeNode(Node *node)
{
    if (!node)
        return;
    for (Node *child : node->children)
        freeNode(child);
    delete node;
}

ProcessTreeModel::Node *ProcessTreeModel::nodeFromIndex(const QModelIndex &index)
{
    if (!index.isValid())
        return nullptr;
    return static_cast<Node *>(index.internalPointer());
}
