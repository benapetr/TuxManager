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

#include "sidepanel.h"
#include "../colorscheme.h"

using namespace Perf;

SidePanel::SidePanel(QWidget *parent) : QWidget(parent)
    , m_scrollArea(new QScrollArea(this))
    , m_container(new QWidget)
    , m_containerLayout(new QVBoxLayout(this->m_container))
{
    // Container inside the scroll area
    this->m_containerLayout->setContentsMargins(0, 0, 0, 0);
    this->m_containerLayout->setSpacing(1);
    this->m_containerLayout->addStretch(1);      // pushes items to the top

    this->m_scrollArea->setWidget(this->m_container);
    this->m_scrollArea->setWidgetResizable(true);
    this->m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    this->m_scrollArea->setFrameShape(QFrame::NoFrame);

    QPalette pal = this->m_container->palette();
    pal.setColor(QPalette::Window, ColorScheme::GetCurrent()->SidePanelBackgroundColor);
    this->m_container->setPalette(pal);
    this->m_container->setAutoFillBackground(true);

    QVBoxLayout *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);
    outerLayout->addWidget(this->m_scrollArea);
    this->setLayout(outerLayout);

    this->setMinimumWidth(150);
}

void SidePanel::ApplyColorScheme()
{
    QPalette pal = this->m_container->palette();
    pal.setColor(QPalette::Window, ColorScheme::GetCurrent()->SidePanelBackgroundColor);
    this->m_container->setPalette(pal);
    this->m_container->update();
    this->update();
}

void SidePanel::SetItemOrder(const QList<SidePanelItem *> &items)
{
    if (items.size() != this->m_items.size())
        return;

    for (SidePanelItem *item : items)
    {
        if (!item || !this->m_items.contains(item))
            return;
    }

    for (SidePanelItem *item : this->m_items)
        this->m_containerLayout->removeWidget(item);

    this->m_items = items;

    const int stretchIdx = this->m_containerLayout->count() - 1;
    for (int i = 0; i < this->m_items.size(); ++i)
        this->m_containerLayout->insertWidget(stretchIdx + i, this->m_items.at(i));
}

int SidePanel::AddItem(SidePanelItem *item)
{
    const int index = this->m_items.size();
    this->m_items.append(item);

    // Insert before the trailing stretch
    const int stretchIdx = this->m_containerLayout->count() - 1;
    this->m_containerLayout->insertWidget(stretchIdx, item);

    connect(item, &SidePanelItem::clicked, this, [this, item]()
    {
        this->SetCurrentItem(item);
    });
    connect(item, &SidePanelItem::contextMenuRequested, this, [this, item](const QPoint &globalPos)
    {
        emit this->itemContextMenuRequested(item, globalPos);
    });

    // Auto-select the first item added
    if (index == 0)
        this->SetCurrentItem(item);

    return index;
}

void SidePanel::SetCurrentItem(SidePanelItem *item)
{
    if (!item || !this->m_items.contains(item))
        return;
    if (!this->IsItemVisible(item))
        return;
    if (item == this->m_currentItem)
        return;

    // Deselect previous
    if (this->m_currentItem)
        this->m_currentItem->SetSelected(false);

    this->m_currentItem = item;
    this->m_currentItem->SetSelected(true);

    emit this->currentChanged(item);
}

void SidePanel::SetItemVisible(SidePanelItem *item, bool visible)
{
    if (!item)
        return;

    item->setVisible(visible);
    if (!visible && this->m_currentItem == item)
    {
        SidePanelItem *next = this->FirstVisibleItem();
        if (next)
        {
            this->SetCurrentItem(next);
        } else
        {
            this->m_currentItem->SetSelected(false);
            this->m_currentItem = nullptr;
        }
    }
}

bool SidePanel::IsItemVisible(SidePanelItem *item) const
{
    return item && item->isVisible();
}

SidePanelItem *SidePanel::FirstVisibleItem() const
{
    for (SidePanelItem *item : this->m_items)
    {
        if (item && item->isVisible())
            return item;
    }
    return nullptr;
}
