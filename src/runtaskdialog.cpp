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
#include "globals.h"
#include "logger.h"

#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QLineEdit>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QStandardPaths>

namespace
{
    QString expandUserPath(QString path)
    {
        if (path == QStringLiteral("~"))
            return QDir::homePath();
        if (path.startsWith(QStringLiteral("~/")))
            return QDir::homePath() + path.mid(1);
        return path;
    }

    QString resolveExecutablePath(const QString &program)
    {
        const QString expandedProgram = expandUserPath(program);
        if (expandedProgram.contains('/'))
            return expandedProgram;
        return QStandardPaths::findExecutable(expandedProgram);
    }
}

RunTaskDialog::RunTaskDialog(QWidget *parent) : QDialog(parent), ui(new Ui::RunTaskDialog)
{
    this->ui->setupUi(this);

    this->ui->commandCombo->addItems(CFG->TaskHistory);
    this->ui->commandCombo->setCurrentText(QString());
    this->ui->commandCombo->setFocus();

    this->m_okButton = this->ui->buttonBox->button(QDialogButtonBox::Ok);

    connect(this->ui->buttonBox, &QDialogButtonBox::accepted, this, &RunTaskDialog::accept);
    connect(this->ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(this->ui->browseButton, &QPushButton::clicked, this, &RunTaskDialog::browseForCommand);
    connect(this->ui->commandCombo->lineEdit(), &QLineEdit::textChanged, this, [this]()
    {
        this->updateOkButton();
    });

    this->updateOkButton();
}

RunTaskDialog::~RunTaskDialog()
{
    delete this->ui;
}

QString RunTaskDialog::GetCommand() const
{
    return this->ui->commandCombo->currentText().trimmed();
}

void RunTaskDialog::accept()
{
    const QString command = this->GetCommand();
    if (command.isEmpty())
        return;

    QString error;
    if (!this->startDetachedCommand(command, &error))
    {
        QMessageBox::warning(this, tr("Run new task failed"), tr("Failed to start command.\n\n%1").arg(error));
        return;
    }

    CFG->TaskHistory.removeAll(command);
    CFG->TaskHistory.prepend(command);
    while (CFG->TaskHistory.size() > TUX_MANAGER_TASK_HISTORY)
        CFG->TaskHistory.removeLast();
    CFG->Save();

    LOG_INFO(QString("Started detached task: %1").arg(command));
    QDialog::accept();
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
        this->m_okButton->setEnabled(!this->GetCommand().isEmpty());
}

bool RunTaskDialog::startDetachedCommand(const QString &command, QString *error) const
{
    if (error)
        error->clear();

    const QString trimmed = command.trimmed();
    if (trimmed.isEmpty())
    {
        if (error)
            *error = tr("Command is empty");
        return false;
    }

    const QStringList parts = QProcess::splitCommand(trimmed);
    if (parts.isEmpty())
    {
        if (error)
            *error = tr("Command is empty");
        return false;
    }

    const QString program = parts.constFirst();
    const QString executable = resolveExecutablePath(program);
    if (executable.isEmpty())
    {
        if (error)
            *error = tr("Executable was not found: %1").arg(program);
        return false;
    }

    const QFileInfo executableInfo(executable);
    if (!executableInfo.exists())
    {
        if (error)
            *error = tr("Executable does not exist: %1").arg(executable);
        return false;
    }

    if (!executableInfo.isExecutable())
    {
        if (error)
            *error = tr("File is not executable: %1").arg(executable);
        return false;
    }

    QStringList arguments = parts;
    arguments.removeFirst();
    if (QProcess::startDetached(executable, arguments))
        return true;

    if (error)
        *error = tr("Failed to start executable: %1").arg(executable);
    return false;
}

QString RunTaskDialog::shellQuote(const QString &text)
{
    QString escaped = text;
    escaped.replace('\'', QStringLiteral("'\"'\"'"));
    return QStringLiteral("'%1'").arg(escaped);
}
