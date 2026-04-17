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

#include "colorschemedialog.h"
#include "ui_colorschemedialog.h"
#include "configuration.h"

#include <QColorDialog>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QPushButton>
#include <utility>

ColorSchemeDialog::ColorSchemeDialog(QWidget *parent) : QDialog(parent), ui(new Ui::ColorSchemeDialog)
{
    this->ui->setupUi(this);

    this->m_defaultScheme = ColorScheme::DetectDarkMode()
                            ? ColorScheme::DefaultDark()
                            : ColorScheme::DefaultLight();
    this->m_scheme = CFG->UseCustomColorScheme
                     ? *ColorScheme::GetCurrent()
                     : this->m_defaultScheme;
    this->ui->customCheck->setChecked(CFG->UseCustomColorScheme);

    const QVector<ColorScheme::ColorField> &fields = ColorScheme::Fields();
    this->m_rows.reserve(fields.size());
    for (const ColorScheme::ColorField &field : fields)
    {
        const ColorScheme::ColorField fieldCopy = field;
        auto *rowHost = new QWidget(this->ui->scrollAreaWidgetContents);
        auto *rowLayout = new QHBoxLayout(rowHost);
        rowLayout->setContentsMargins(0, 0, 0, 0);

        auto *preview = new QFrame(rowHost);
        preview->setFrameShape(QFrame::StyledPanel);
        preview->setFixedSize(36, 20);

        auto *pick = new QPushButton(tr("Choose"), rowHost);
        auto *reset = new QPushButton(tr("Reset"), rowHost);

        rowLayout->addWidget(preview);
        rowLayout->addWidget(pick);
        rowLayout->addWidget(reset);
        rowLayout->addStretch(1);

        this->ui->formLayout->addRow(labelForField(QString::fromLatin1(field.Name)), rowHost);
        this->m_rows.append({ &field, preview, pick, reset });

        connect(pick, &QPushButton::clicked, this, [this, fieldCopy]()
        {
            const QColor current = this->m_scheme.*(fieldCopy.Member);
            const QColor picked = QColorDialog::getColor(current, this, labelForField(QString::fromLatin1(fieldCopy.Name)));
            if (picked.isValid())
                this->setColor(fieldCopy, picked);
        });

        connect(reset, &QPushButton::clicked, this, [this, fieldCopy]()
        {
            this->setColor(fieldCopy, this->m_defaultScheme.*(fieldCopy.Member));
        });
    }

    connect(this->ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(this->ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(this->ui->customCheck, &QCheckBox::toggled, this, [this]() { this->refreshUi(); });
    this->refreshUi();
}

ColorSchemeDialog::~ColorSchemeDialog()
{
    delete this->ui;
}

bool ColorSchemeDialog::UseCustomScheme() const
{
    return this->ui->customCheck->isChecked();
}

ColorScheme ColorSchemeDialog::BuildScheme() const
{
    return this->m_scheme;
}

void ColorSchemeDialog::refreshUi()
{
    const bool enabled = this->ui->customCheck->isChecked();
    for (const RowWidgets &row : std::as_const(this->m_rows))
    {
        row.Preview->setEnabled(enabled);
        row.PickButton->setEnabled(enabled);
        row.ResetButton->setEnabled(enabled);

        const QColor color = this->m_scheme.*(row.Field->Member);
        row.Preview->setStyleSheet(QString("background:%1; border:1px solid palette(mid);").arg(color.name(QColor::HexArgb)));
        row.PickButton->setText(color.name(QColor::HexArgb));
    }
}

void ColorSchemeDialog::setColor(const ColorScheme::ColorField &field, const QColor &color)
{
    this->m_scheme.*(field.Member) = color;
    this->refreshUi();
}

QString ColorSchemeDialog::labelForField(const QString &name)
{
    QString label;
    label.reserve(name.size() + 8);
    for (int i = 0; i < name.size(); ++i)
    {
        const QChar c = name.at(i);
        if (i > 0 && c.isUpper())
            label += ' ';
        label += c;
    }
    if (label.endsWith(" Color"))
        label.chop(6);
    return label;
}
