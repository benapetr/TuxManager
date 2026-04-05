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

using namespace Perf;

SidePanel::SidePanel(QWidget *parent)
    : QWidget(parent)
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

    // Keep original dark styling in dark mode, adapt to palette in light mode.
    QPalette pal = this->m_container->palette();
    const QColor window = this->palette().color(QPalette::Window);
    const bool darkTheme = window.lightness() <= 127;
    pal.setColor(QPalette::Window, darkTheme ? QColor(0x12, 0x12, 0x1a) : window);
    this->m_container->setPalette(pal);
    this->m_container->setAutoFillBackground(true);

    QVBoxLayout *outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setSpacing(0);
    outerLayout->addWidget(this->m_scrollArea);
    this->setLayout(outerLayout);

    this->setMinimumWidth(150);
    this->setMaximumWidth(200);
}

int SidePanel::AddItem(SidePanelItem *item)
{
    const int index = this->m_items.size();
    this->m_items.append(item);

    // Insert before the trailing stretch
    const int stretchIdx = this->m_containerLayout->count() - 1;
    this->m_containerLayout->insertWidget(stretchIdx, item);

    connect(item, &SidePanelItem::clicked, this, [this, index]()
    {
        this->SetCurrentIndex(index);
    });
    connect(item, &SidePanelItem::contextMenuRequested, this, [this, index](const QPoint &globalPos)
    {
        emit this->itemContextMenuRequested(index, globalPos);
    });

    // Auto-select the first item added
    if (index == 0)
        this->SetCurrentIndex(0);

    return index;
}

void SidePanel::SetCurrentIndex(int index)
{
    if (index < 0 || index >= this->m_items.size())
        return;
    if (!this->IsItemVisible(index))
        return;
    if (index == this->m_currentIndex)
        return;

    // Deselect previous
    if (this->m_currentIndex >= 0 && this->m_currentIndex < this->m_items.size())
        this->m_items.at(this->m_currentIndex)->SetSelected(false);

    this->m_currentIndex = index;
    this->m_items.at(index)->SetSelected(true);

    emit this->currentChanged(index);
}

void SidePanel::SetItemVisible(int index, bool visible)
{
    SidePanelItem *item = this->GetItemAt(index);
    if (!item)
        return;

    item->setVisible(visible);
    if (!visible && this->m_currentIndex == index)
    {
        const int next = this->FirstVisibleIndex();
        if (next >= 0)
            this->SetCurrentIndex(next);
        else
            this->m_currentIndex = -1;
    }
}

bool SidePanel::IsItemVisible(int index) const
{
    SidePanelItem *item = this->GetItemAt(index);
    return item && item->isVisible();
}

int SidePanel::FirstVisibleIndex() const
{
    for (int i = 0; i < this->m_items.size(); ++i)
    {
        if (this->m_items.at(i)->isVisible())
            return i;
    }
    return -1;
}

SidePanelItem *SidePanel::GetItemAt(int index) const
{
    if (index < 0 || index >= this->m_items.size())
        return nullptr;
    return this->m_items.at(index);
}
