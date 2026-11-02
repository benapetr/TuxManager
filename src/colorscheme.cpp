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

#include "colorscheme.h"

#include <QApplication>
#include <QPalette>
#include <QVariant>

ColorScheme *ColorScheme::current = nullptr;

namespace
{
    const ColorScheme::ColorField kColorFields[] =
    {
        { "CpuGraphLineColor", &ColorScheme::CpuGraphLineColor },
        { "CpuGraphFillColor", &ColorScheme::CpuGraphFillColor },
        { "CpuGraphSecondaryFillColor", &ColorScheme::CpuGraphSecondaryFillColor },
        { "MemoryGraphLineColor", &ColorScheme::MemoryGraphLineColor },
        { "MemoryGraphFillColor", &ColorScheme::MemoryGraphFillColor },
        { "DiskGraphLineColor", &ColorScheme::DiskGraphLineColor },
        { "DiskGraphFillColor", &ColorScheme::DiskGraphFillColor },
        { "DiskTransferGraphLineColor", &ColorScheme::DiskTransferGraphLineColor },
        { "DiskTransferGraphFillColor", &ColorScheme::DiskTransferGraphFillColor },
        { "DiskTransferGraphSecondaryFillColor", &ColorScheme::DiskTransferGraphSecondaryFillColor },
        { "NetworkGraphLineColor", &ColorScheme::NetworkGraphLineColor },
        { "NetworkGraphFillColor", &ColorScheme::NetworkGraphFillColor },
        { "NetworkGraphSecondaryFillColor", &ColorScheme::NetworkGraphSecondaryFillColor },
        { "GpuGraphLineColor", &ColorScheme::GpuGraphLineColor },
        { "GpuGraphFillColor", &ColorScheme::GpuGraphFillColor },
        { "GpuGraphSecondaryFillColor", &ColorScheme::GpuGraphSecondaryFillColor },
        { "SwapUsageGraphLineColor", &ColorScheme::SwapUsageGraphLineColor },
        { "SwapUsageGraphFillColor", &ColorScheme::SwapUsageGraphFillColor },
        { "SwapActivityGraphLineColor", &ColorScheme::SwapActivityGraphLineColor },
        { "SwapActivityGraphFillColor", &ColorScheme::SwapActivityGraphFillColor },
        { "SwapActivityGraphSecondaryFillColor", &ColorScheme::SwapActivityGraphSecondaryFillColor },
        { "GraphGridColor", &ColorScheme::GraphGridColor },
        { "GraphOverlayTextColor", &ColorScheme::GraphOverlayTextColor },
        { "SidePanelBackgroundColor", &ColorScheme::SidePanelBackgroundColor },
        { "SidePanelItemSelectedBackgroundColor", &ColorScheme::SidePanelItemSelectedBackgroundColor },
        { "SidePanelItemHoverBackgroundColor", &ColorScheme::SidePanelItemHoverBackgroundColor },
        { "SidePanelItemBackgroundColor", &ColorScheme::SidePanelItemBackgroundColor },
        { "SidePanelItemSelectedTextColor", &ColorScheme::SidePanelItemSelectedTextColor },
        { "SidePanelItemTextColor", &ColorScheme::SidePanelItemTextColor },
        { "SidePanelItemSubtitleColor", &ColorScheme::SidePanelItemSubtitleColor },
        { "SidePanelItemSelectedBorderColor", &ColorScheme::SidePanelItemSelectedBorderColor },
        { "CpuTitleColor", &ColorScheme::CpuTitleColor },
        { "CpuHeaderValueColor", &ColorScheme::CpuHeaderValueColor },
        { "MemoryTitleColor", &ColorScheme::MemoryTitleColor },
        { "MemoryHeaderValueColor", &ColorScheme::MemoryHeaderValueColor },
        { "DiskTitleColor", &ColorScheme::DiskTitleColor },
        { "DiskHeaderValueColor", &ColorScheme::DiskHeaderValueColor },
        { "NetworkTitleColor", &ColorScheme::NetworkTitleColor },
        { "GpuTitleColor", &ColorScheme::GpuTitleColor },
        { "MutedTextColor", &ColorScheme::MutedTextColor },
        { "StatLabelColor", &ColorScheme::StatLabelColor },
        { "AxisLabelColor", &ColorScheme::AxisLabelColor },
        { "MemoryLegendTextColor", &ColorScheme::MemoryLegendTextColor },
        { "MemoryLegendUsedColor", &ColorScheme::MemoryLegendUsedColor },
        { "MemoryLegendCompressedColor", &ColorScheme::MemoryLegendCompressedColor },
        { "MemoryLegendDirtyColor", &ColorScheme::MemoryLegendDirtyColor },
        { "MemoryLegendCachedColor", &ColorScheme::MemoryLegendCachedColor },
        { "MemoryLegendFreeColor", &ColorScheme::MemoryLegendFreeColor },
        { "MemoryBarUsedColor", &ColorScheme::MemoryBarUsedColor },
        { "MemoryBarCompressedColor", &ColorScheme::MemoryBarCompressedColor },
        { "MemoryBarDirtyColor", &ColorScheme::MemoryBarDirtyColor },
        { "MemoryBarCachedColor", &ColorScheme::MemoryBarCachedColor },
        { "MemoryBarFreeColor", &ColorScheme::MemoryBarFreeColor },
        { "MemoryBarBorderColor", &ColorScheme::MemoryBarBorderColor }
    };

    QColor colorFromVariant(const QVariant &value, const QColor &fallback)
    {
        if (!value.isValid())
            return fallback;
        if (value.canConvert<QColor>())
        {
            const QColor color = value.value<QColor>();
            if (color.isValid())
                return color;
        }

        const QString text = value.toString().trimmed();
        if (text.isEmpty())
            return fallback;

        const QColor color(text);
        return color.isValid() ? color : fallback;
    }
}

ColorScheme *ColorScheme::GetCurrent()
{
    if (!ColorScheme::current)
        ColorScheme::current = new ColorScheme(ColorScheme::DefaultLight());
    return ColorScheme::current;
}

bool ColorScheme::DetectDarkMode()
{
    return QApplication::palette().color(QPalette::Window).lightness() <= 127;
}

const QVector<ColorScheme::ColorField> &ColorScheme::Fields()
{
    static const QVector<ColorScheme::ColorField> fields = []
    {
        QVector<ColorScheme::ColorField> items;
        items.reserve(static_cast<int>(sizeof(kColorFields) / sizeof(kColorFields[0])));
        for (const ColorScheme::ColorField &field : kColorFields)
            items.append(field);
        return items;
    }();
    return fields;
}

void ColorScheme::Install(ColorScheme *scheme)
{
    delete ColorScheme::current;
    ColorScheme::current = scheme;
}

ColorScheme::ColorScheme()
{}

ColorScheme ColorScheme::DefaultDark()
{
    ColorScheme scheme;
    scheme.DarkMode = true;

    scheme.CpuGraphLineColor = QColor(0x00, 0xbc, 0xff);              // bright cyan
    scheme.CpuGraphFillColor = QColor(0x00, 0x4c, 0x8a, 120);         // dark blue, semi-transparent
    scheme.CpuGraphSecondaryFillColor = QColor(0x00, 0x22, 0x55, 160); // very dark navy, semi-transparent
    scheme.MemoryGraphLineColor = QColor(0xcc, 0x44, 0xcc);            // medium purple
    scheme.MemoryGraphFillColor = QColor(0x66, 0x11, 0x66, 130);       // dark purple, semi-transparent
    scheme.DiskGraphLineColor = QColor(0x66, 0xbb, 0x44);              // medium green
    scheme.DiskGraphFillColor = QColor(0x33, 0x66, 0x22, 120);         // dark green, semi-transparent
    scheme.DiskTransferGraphLineColor = QColor(0x88, 0xcc, 0x66);      // light green
    scheme.DiskTransferGraphFillColor = QColor(0x33, 0x66, 0x22, 100); // dark green, semi-transparent
    scheme.DiskTransferGraphSecondaryFillColor = QColor(0x1f, 0x44, 0x15, 120); // very dark green, semi-transparent
    scheme.NetworkGraphLineColor = QColor(0xdb, 0x8b, 0x3a);           // orange
    scheme.NetworkGraphFillColor = QColor(0x66, 0x3f, 0x1f, 110);      // dark brown, semi-transparent
    scheme.NetworkGraphSecondaryFillColor = QColor(0x4a, 0x28, 0x10, 130); // very dark brown, semi-transparent
    scheme.GpuGraphLineColor = QColor(0x44, 0xa8, 0xff);               // light blue
    scheme.GpuGraphFillColor = QColor(0x1e, 0x4d, 0x82, 110);          // dark blue, semi-transparent
    scheme.GpuGraphSecondaryFillColor = QColor(0x14, 0x33, 0x58, 130); // very dark blue, semi-transparent
    scheme.SwapUsageGraphLineColor = QColor(0xcc, 0x88, 0x44);         // amber
    scheme.SwapUsageGraphFillColor = QColor(0x66, 0x33, 0x11, 120);    // dark brown, semi-transparent
    scheme.SwapActivityGraphLineColor = QColor(0xcc, 0xaa, 0x66);      // tan/gold
    scheme.SwapActivityGraphFillColor = QColor(0x66, 0x44, 0x22, 100); // dark tan, semi-transparent
    scheme.SwapActivityGraphSecondaryFillColor = QColor(0x4a, 0x2d, 0x14, 120); // very dark brown, semi-transparent
    scheme.GraphGridColor = QColor(0x88, 0x88, 0x99, 150);             // grey-blue, semi-transparent
    scheme.GraphOverlayTextColor = QColor(245, 245, 245, 220);         // near white, semi-transparent
    scheme.SidePanelBackgroundColor = QColor(0x12, 0x12, 0x1a);        // very dark navy
    scheme.SidePanelItemSelectedBackgroundColor = QColor(0x1a, 0x4a, 0x8a, 200); // dark blue, semi-transparent
    scheme.SidePanelItemHoverBackgroundColor = QColor(0x30, 0x30, 0x40, 120);    // dark grey-blue, semi-transparent
    scheme.SidePanelItemBackgroundColor = QColor(0x1a, 0x1a, 0x22, 180);         // very dark grey, semi-transparent
    scheme.SidePanelItemSelectedTextColor = QColor(0xff, 0xff, 0xff);   // white
    scheme.SidePanelItemTextColor = QColor(0xcc, 0xcc, 0xcc);           // light grey
    scheme.SidePanelItemSubtitleColor = QColor(0x88, 0xaa, 0xcc);       // steel blue
    scheme.SidePanelItemSelectedBorderColor = QColor(0x44, 0x88, 0xff); // bright blue
    scheme.CpuTitleColor = scheme.CpuGraphLineColor;
    scheme.CpuHeaderValueColor = QColor(0xaa, 0xcc, 0xff);              // pale blue
    scheme.MemoryTitleColor = scheme.MemoryGraphLineColor;
    scheme.MemoryHeaderValueColor = QColor(0xdd, 0xaa, 0xdd);           // pale violet
    scheme.DiskTitleColor = scheme.DiskGraphLineColor;
    scheme.DiskHeaderValueColor = QColor(0xaa, 0xdd, 0xaa);             // pale green
    scheme.NetworkTitleColor = scheme.NetworkGraphLineColor;
    scheme.GpuTitleColor = scheme.GpuGraphLineColor;
    scheme.MutedTextColor = QColor(0xaa, 0xaa, 0xaa);                   // medium grey
    scheme.StatLabelColor = QColor(0x88, 0x88, 0x88);                   // grey
    scheme.AxisLabelColor = QColor(0x66, 0x66, 0x66);                   // dark grey
    scheme.MemoryLegendTextColor = QColor(0xaa, 0xaa, 0xaa);            // medium grey
    scheme.MemoryLegendUsedColor = scheme.MemoryGraphLineColor;
    scheme.MemoryLegendCompressedColor = QColor(0xaa, 0x66, 0xaa);      // muted purple
    scheme.MemoryLegendDirtyColor = QColor(0xbb, 0x88, 0x00);           // amber/dark yellow
    scheme.MemoryLegendCachedColor = QColor(0x55, 0x22, 0x55);          // dark purple
    scheme.MemoryLegendFreeColor = QColor(0x33, 0x33, 0x33);            // very dark grey
    scheme.MemoryBarUsedColor = scheme.MemoryGraphLineColor;
    scheme.MemoryBarCompressedColor = QColor(0x77, 0x44, 0x77);         // dark purple
    scheme.MemoryBarDirtyColor = QColor(0xbb, 0x88, 0x00);              // amber/dark yellow
    scheme.MemoryBarCachedColor = QColor(0x55, 0x22, 0x55);             // dark purple
    scheme.MemoryBarFreeColor = QColor(0x11, 0x08, 0x11);               // near black
    scheme.MemoryBarBorderColor = QColor(0x88, 0x44, 0x88);             // muted purple
    return scheme;
}

ColorScheme ColorScheme::DefaultLight()
{
    ColorScheme scheme;
    scheme.DarkMode = false;

    scheme.CpuGraphLineColor = QColor(0x00, 0x8f, 0xcc);              // medium blue
    scheme.CpuGraphFillColor = QColor(0x99, 0xd9, 0xff, 120);         // pale sky blue, semi-transparent
    scheme.CpuGraphSecondaryFillColor = QColor(0x5c, 0xb9, 0xec, 130); // light blue, semi-transparent
    scheme.MemoryGraphLineColor = QColor(0xb0, 0x3d, 0xb0);            // medium purple
    scheme.MemoryGraphFillColor = QColor(0xe7, 0xba, 0xe7, 130);       // pale lavender, semi-transparent
    scheme.DiskGraphLineColor = QColor(0x5a, 0x9d, 0x3b);              // medium green
    scheme.DiskGraphFillColor = QColor(0xc9, 0xe1, 0xbf, 120);         // pale green, semi-transparent
    scheme.DiskTransferGraphLineColor = QColor(0x77, 0xb8, 0x4f);      // medium-light green
    scheme.DiskTransferGraphFillColor = QColor(0xd8, 0xea, 0xcf, 110); // very pale green, semi-transparent
    scheme.DiskTransferGraphSecondaryFillColor = QColor(0xbc, 0xd8, 0xad, 125); // light sage green, semi-transparent
    scheme.NetworkGraphLineColor = QColor(0xc8, 0x74, 0x1d);           // burnt orange
    scheme.NetworkGraphFillColor = QColor(0xf0, 0xcd, 0xaa, 115);      // pale peach, semi-transparent
    scheme.NetworkGraphSecondaryFillColor = QColor(0xe2, 0xb1, 0x7a, 125); // light tan/orange, semi-transparent
    scheme.GpuGraphLineColor = QColor(0x2e, 0x87, 0xd1);               // medium blue
    scheme.GpuGraphFillColor = QColor(0xb6, 0xd6, 0xf3, 115);          // pale blue, semi-transparent
    scheme.GpuGraphSecondaryFillColor = QColor(0x8b, 0xbf, 0xe9, 125); // light blue, semi-transparent
    scheme.SwapUsageGraphLineColor = QColor(0xb4, 0x76, 0x35);         // medium brown
    scheme.SwapUsageGraphFillColor = QColor(0xe7, 0xc9, 0xaa, 120);    // pale tan, semi-transparent
    scheme.SwapActivityGraphLineColor = QColor(0xb3, 0x8d, 0x4b);      // golden tan
    scheme.SwapActivityGraphFillColor = QColor(0xe8, 0xd8, 0xb8, 110); // pale gold, semi-transparent
    scheme.SwapActivityGraphSecondaryFillColor = QColor(0xd5, 0xb8, 0x88, 125); // light gold, semi-transparent
    scheme.GraphGridColor = QColor(0x80, 0x80, 0x80, 72);              // grey, semi-transparent
    scheme.GraphOverlayTextColor = QColor(35, 35, 35, 220);            // near black, semi-transparent
    scheme.SidePanelBackgroundColor = QColor(0xf3, 0xf5, 0xf8);        // very light grey-blue
    scheme.SidePanelItemSelectedBackgroundColor = QColor(0x66, 0xa8, 0xff, 96); // medium blue, semi-transparent
    scheme.SidePanelItemHoverBackgroundColor = QColor(0x00, 0x00, 0x00, 20);    // black, nearly transparent
    scheme.SidePanelItemBackgroundColor = QColor(0xff, 0xff, 0xff);     // white
    scheme.SidePanelItemSelectedTextColor = QColor(0x12, 0x24, 0x36);   // very dark navy
    scheme.SidePanelItemTextColor = QColor(0x24, 0x24, 0x24);           // very dark grey
    scheme.SidePanelItemSubtitleColor = QColor(0x5e, 0x7a, 0x96);       // steel blue-grey
    scheme.SidePanelItemSelectedBorderColor = QColor(0x44, 0x88, 0xff); // bright blue
    scheme.CpuTitleColor = scheme.CpuGraphLineColor;
    scheme.CpuHeaderValueColor = QColor(0x5d, 0x84, 0xaa);              // steel blue
    scheme.MemoryTitleColor = scheme.MemoryGraphLineColor;
    scheme.MemoryHeaderValueColor = QColor(0xb5, 0x7f, 0xb5);           // muted purple
    scheme.DiskTitleColor = scheme.DiskGraphLineColor;
    scheme.DiskHeaderValueColor = QColor(0x74, 0xa5, 0x5d);             // medium green
    scheme.NetworkTitleColor = scheme.NetworkGraphLineColor;
    scheme.GpuTitleColor = scheme.GpuGraphLineColor;
    scheme.MutedTextColor = QColor(0x77, 0x77, 0x77);                   // medium grey
    scheme.StatLabelColor = QColor(0x7a, 0x7a, 0x7a);                   // medium grey
    scheme.AxisLabelColor = QColor(0x6d, 0x6d, 0x6d);                   // dark grey
    scheme.MemoryLegendTextColor = QColor(0x77, 0x77, 0x77);            // medium grey
    scheme.MemoryLegendUsedColor = scheme.MemoryGraphLineColor;
    scheme.MemoryLegendCompressedColor = QColor(0xb4, 0x73, 0xb4);      // medium purple
    scheme.MemoryLegendDirtyColor = QColor(0xb0, 0x85, 0x23);           // golden amber
    scheme.MemoryLegendCachedColor = QColor(0x9b, 0x75, 0x9b);          // muted purple
    scheme.MemoryLegendFreeColor = QColor(0x8c, 0x8c, 0x8c);            // medium grey
    scheme.MemoryBarUsedColor = QColor(0xc9, 0x7f, 0xc9);               // medium purple
    scheme.MemoryBarCompressedColor = QColor(0xbc, 0x93, 0xbc);         // light purple
    scheme.MemoryBarDirtyColor = QColor(0xd1, 0xa0, 0x3e);              // golden amber
    scheme.MemoryBarCachedColor = QColor(0xd7, 0xbf, 0xd7);             // pale lavender
    scheme.MemoryBarFreeColor = QColor(0xe9, 0xe9, 0xe9);               // very light grey
    scheme.MemoryBarBorderColor = QColor(0xb0, 0x93, 0xb0);             // muted lavender
    return scheme;
}

QVariantMap ColorScheme::ToVariantMap() const
{
    QVariantMap map;
    map.insert("DarkMode", this->DarkMode);
    for (const ColorScheme::ColorField &field : ColorScheme::Fields())
        map.insert(field.Name, (this->*field.Member).name(QColor::HexArgb));
    return map;
}

void ColorScheme::ApplyVariantMap(const QVariantMap &map)
{
    this->DarkMode = map.value("DarkMode", this->DarkMode).toBool();
    for (const ColorScheme::ColorField &field : ColorScheme::Fields())
        this->*field.Member = colorFromVariant(map.value(field.Name), this->*field.Member);
}
