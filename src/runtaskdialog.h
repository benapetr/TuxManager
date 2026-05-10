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

#ifndef RUNTASKDIALOG_H
#define RUNTASKDIALOG_H

#include <QDialog>

QT_BEGIN_NAMESPACE
namespace Ui { class RunTaskDialog; }
class QPushButton;
QT_END_NAMESPACE

class RunTaskDialog : public QDialog
{
    Q_OBJECT

    public:
        explicit RunTaskDialog(QWidget *parent = nullptr);
        ~RunTaskDialog();

        QString GetCommand() const;

    protected:
        void accept() override;

    private:
        bool startDetachedCommand(const QString &command, QString *error) const;
        void browseForCommand();
        void updateOkButton();
        static QString shellQuote(const QString &text);

        Ui::RunTaskDialog *ui { nullptr };
        QPushButton *m_okButton { nullptr };
};

#endif // RUNTASKDIALOG_H
