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

#include "runtaskdialog.h"
#include "ui_runtaskdialog.h"
#include "configuration.h"

#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStandardPaths>
#include <QVBoxLayout>

RunTaskDialog::RunTaskDialog(QWidget *parent) : QDialog(parent), ui(new Ui::RunTaskDialog)
{
    this->ui->setupUi(this);

    this->ui->commandCombo->addItems(CFG->TaskHistory);
    this->ui->commandCombo->setCurrentText(QString());
    this->ui->commandCombo->setFocus();

    this->m_okButton = this->ui->buttonBox->button(QDialogButtonBox::Ok);

    this->m_errorLabel = new QLabel(this);
    this->m_errorLabel->setWordWrap(true);
    this->m_errorLabel->setVisible(false);

    auto *layout = qobject_cast<QVBoxLayout *>(this->layout());
    if (layout)
    {
        const int buttonBoxIndex = layout->indexOf(this->ui->buttonBox);
        layout->insertWidget(buttonBoxIndex, this->m_errorLabel);
    }

    connect(this->ui->buttonBox, &QDialogButtonBox::accepted, this, &RunTaskDialog::accept);
    connect(this->ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(this->ui->browseButton, &QPushButton::clicked, this, &RunTaskDialog::browseForCommand);
    connect(this->ui->commandCombo->lineEdit(), &QLineEdit::textChanged, this, [this]()
    {
        this->m_errorLabel->setVisible(false);
        this->updateOkButton();
    });

    this->updateOkButton();
}

RunTaskDialog::~RunTaskDialog()
{
    delete this->ui;
}

QString RunTaskDialog::Command() const
{
    return this->ui->commandCombo->currentText().trimmed();
}

void RunTaskDialog::browseForCommand()
{
    QString startDir = CFG->LastTaskDirectory;
    if (startDir.isEmpty() || !QDir(startDir).exists())
        startDir = QStringLiteral("/usr/bin");

    const QString path = QFileDialog::getOpenFileName(this, tr("Select executable"), startDir, tr("All files (*)"));
    if (path.isEmpty())
        return;

    CFG->LastTaskDirectory = QFileInfo(path).absolutePath();
    CFG->Save();
    this->ui->commandCombo->setCurrentText(shellQuote(path));
}

void RunTaskDialog::updateOkButton()
{
    if (this->m_okButton)
        this->m_okButton->setEnabled(!this->Command().isEmpty());
}

void RunTaskDialog::accept()
{
    const QString command = this->Command();
    if (command.isEmpty())
        return;

    const QString exe = RunTaskDialog::resolveExecutable(command);
    if (exe.isEmpty())
    {
        // clamped to avoid blowing up the dialog
        QString program = command.split(' ', Qt::SkipEmptyParts).value(0);
        constexpr int maxLen = 40;
        if (program.size() > maxLen)
            program = program.left(maxLen) + QStringLiteral("\u2026"); // '…'
        this->setError(tr("Program or file \u201c%1\u201d does not exist or could not be found.").arg(program));
        return;
    }

    QDialog::accept();
}

void RunTaskDialog::setError(const QString &message)
{
    if (!this->m_errorLabel)
        return;
    this->m_errorLabel->setText(QStringLiteral("\u26a0 ") + message);
    if (!this->m_errorLabel->isVisible())
    {
        this->m_errorLabel->setVisible(true);
        const QSize hint = this->sizeHint();
        if (hint.height() > this->height())
            this->resize(this->width(), hint.height());
    }
}

QString RunTaskDialog::resolveExecutable(const QString &command)
{
    if (command.trimmed().isEmpty())
        return {};

    // Grab the first token
    QString token = command.trimmed();

    // Handle single-quoted token
    if (token.startsWith('\''))
    {
        const int end = token.indexOf('\'', 1);
        token = (end > 0) ? token.mid(1, end - 1) : token.mid(1);
    }
    // Handle double-quoted token
    else if (token.startsWith('"'))
    {
        const int end = token.indexOf('"', 1);
        token = (end > 0) ? token.mid(1, end - 1) : token.mid(1);
    }
    else
    {
        // Take everything up to the first whitespace
        token = token.split(' ', Qt::SkipEmptyParts).value(0);
    }

    if (token.isEmpty())
        return {};

    // Check if the token contains a path separator
    if (token.contains('/') || token.startsWith('.'))
    {
        const QFileInfo fi(token);
        return fi.exists() ? fi.absoluteFilePath() : QString{};
    }

    // Else look it up on PATH
    return QStandardPaths::findExecutable(token);
}

QString RunTaskDialog::shellQuote(const QString &text)
{
    QString escaped = text;
    escaped.replace('\'', QStringLiteral("'\"'\"'"));
    return QStringLiteral("'%1'").arg(escaped);
}
