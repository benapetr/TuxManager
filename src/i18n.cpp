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

#include "i18n.h"
#include "logger.h"

#include <QCoreApplication>
#include <QFile>
#include <QLibraryInfo>
#include <QLocale>
#include <QStringList>
#include <QTranslator>
#include <QtGlobal>

namespace
{
    // Translators live for the whole application lifetime.
    QTranslator *appTranslator()
    {
        static QTranslator *t = new QTranslator();
        return t;
    }

    QTranslator *qtTranslator()
    {
        static QTranslator *t = new QTranslator();
        return t;
    }

    bool isEnglishLanguage(const QString &code)
    {
        if (code.isEmpty())
            return true;
        // "C" / "POSIX" mean the classic unlocalized environment.
        const QString base = code.section('_', 0, 0).toLower();
        return base == QStringLiteral("en")
               || base == QStringLiteral("c")
               || base == QStringLiteral("posix");
    }

    // Load an embedded application catalog. Exact locale match is preferred,
    // then the language part only.
    bool loadCatalog(QTranslator *translator, const QString &catalog, const QString &language)
    {
        QStringList candidates;
        candidates << language;
        const int underscore = language.indexOf(QLatin1Char('_'));
        if (underscore > 0)
            candidates << language.left(underscore);

        for (const QString &lang : candidates)
        {
            const QString path = QStringLiteral(":/i18n/") + catalog + QLatin1Char('_') + lang + QStringLiteral(".qm");
            if (translator->load(path))
            {
                LOG_DEBUG(QString("I18n: loaded embedded catalog %1").arg(path));
                return true;
            }
        }
        return false;
    }

}

void I18n::InstallTranslators()
{
    const QString code = QLocale::system().name();

    // Always drop any previously installed translator first so a restart or
    // repeated call can never stack two translations.
    if (appTranslator()->isEmpty() == false)
        QCoreApplication::removeTranslator(appTranslator());
    if (qtTranslator()->isEmpty() == false)
        QCoreApplication::removeTranslator(qtTranslator());

    if (isEnglishLanguage(code))
    {
        LOG_DEBUG("I18n: no translation needed, using built-in English UI");
        return;
    }

    const QString matched = loadCatalog(appTranslator(), QStringLiteral("tux-manager"), code) ? code : QString();
    if (matched.isEmpty())
    {
        // No translation file for this language, fall back to English UI.
        LOG_INFO(QString("I18n: no translation available for '%1', falling back to English").arg(code));
        return;
    }

    // Best effort: also localize Qt built-in dialogs (QMessageBox buttons etc.)
    // using the matching qtbase catalog from the Qt installation.
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const QString qtTranslationsDir = QLibraryInfo::path(QLibraryInfo::TranslationsPath);
#else
    const QString qtTranslationsDir = QLibraryInfo::location(QLibraryInfo::TranslationsPath);
#endif
    if (QFile::exists(qtTranslationsDir))
    {
        QStringList candidates;
        candidates << matched;
        const int underscore = matched.indexOf(QLatin1Char('_'));
        if (underscore > 0)
            candidates << matched.left(underscore);

        for (const QString &lang : candidates)
        {
            const QString fileName = QStringLiteral("qtbase_") + lang + QStringLiteral(".qm");
            const QString path = qtTranslationsDir.endsWith(QLatin1Char('/')) ? qtTranslationsDir + fileName
                                                                             : qtTranslationsDir + QLatin1Char('/') + fileName;
            if (QFile::exists(path))
            {
                if (qtTranslator()->load(fileName, qtTranslationsDir))
                {
                    QCoreApplication::installTranslator(qtTranslator());
                    LOG_DEBUG(QString("I18n: loaded Qt base translation %1").arg(fileName));
                }
                break;
            }
        }
    }

    QCoreApplication::installTranslator(appTranslator());
    LOG_INFO(QString("I18n: using language '%1'").arg(matched));
}
