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

#include "aboutdialog.h"
#include "globals.h"
#include "ui_aboutdialog.h"

AboutDialog::AboutDialog(QWidget *parent) : QDialog(parent), ui(new Ui::AboutDialog)
{
    this->ui->setupUi(this);
    this->ui->labelTitle->setText(QString::fromLatin1(TUX_MANAGER_PRODUCT_NAME));
    this->ui->labelVersion->setText(tr("版本 %1").arg(QString::fromLatin1(TUX_MANAGER_VERSION_STRING)));
    this->setFixedSize(this->size());
}

AboutDialog::~AboutDialog()
{
    delete this->ui;
}

void AboutDialog::on_closeButton_clicked()
{
    this->close();
}

