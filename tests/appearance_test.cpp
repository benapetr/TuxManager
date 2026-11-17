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

#include "mainwindow.h"
#include "configuration.h"
#include "colorscheme.h"
#include "ui/thememanager.h"
#include <QApplication>
#include <QAction>
#include <QMenu>
#include <QScrollArea>
#include <QSettings>
#include <QStyle>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>
#include <memory>

class AppearanceTest : public QObject
{
    Q_OBJECT
    private:
        std::unique_ptr<MainWindow> window;

        void preference(const QString &nameSpace, const QVariant &value)
        {
            QVERIFY(QMetaObject::invokeMethod(ThemeManager::Get(), "onSettingChanged", Qt::DirectConnection,
                Q_ARG(QString, nameSpace), Q_ARG(QString, QString("color-scheme")),
                Q_ARG(QDBusVariant, QDBusVariant(value))));
        }

        QAction *interval(int ms)
        {
            QMenu *menu = window->findChild<QMenu *>("refreshIntervalMenu");
            if (!menu)
                return nullptr;
            for (QAction *action : menu->actions())
                if (!action->isSeparator() && action->data().toInt() == ms)
                    return action;
            return nullptr;
        }

    private slots:
        void initTestCase()
        {
            QSettings settings;
            settings.setValue("Tests/Isolated", true); // Prevent legacy configuration migration.
            settings.sync();
            CFG->Load();
            CFG->RefreshPaused = true;
            CFG->ForceGpuDrm = true;
            ThemeManager::Get()->Apply();
            if (qEnvironmentVariableIsSet("TUX_TEST_EXPECT_SYSTEM_DARK"))
                QCOMPARE(ColorScheme::DetectDarkMode(), qEnvironmentVariableIntValue("TUX_TEST_EXPECT_SYSTEM_DARK") != 0);
            window.reset(new MainWindow);
            window->show();
            QCoreApplication::processEvents();
        }

        void appearanceActionsAndPersistence()
        {
            for (const QString &mode : { QString("dark"), QString("light"), QString("dark") })
            {
                QAction *action = window->findChild<QAction *>("appearance_" + mode);
                QVERIFY(action);
                action->trigger();
                QCoreApplication::processEvents();
                const bool dark = mode == "dark";
                QCOMPARE(CFG->AppearanceMode, mode);
                QCOMPARE(QSettings().value("General/AppearanceMode").toString(), mode);
                QCOMPARE(ColorScheme::DetectDarkMode(), dark);
                QCOMPARE(ColorScheme::GetCurrent()->DarkMode, dark);
                QCOMPARE(window->palette().color(QPalette::Window).lightness() <= 127, dark);
                const auto areas = window->findChildren<QScrollArea *>();
                QVERIFY(!areas.isEmpty());
                for (QScrollArea *area : areas)
                    QCOMPARE(area->viewport()->palette().color(QPalette::Window).lightness() <= 127, dark);
            }
            CFG->AppearanceMode = "light";
            CFG->Load();
            QCOMPARE(CFG->AppearanceMode, QString("dark"));
        }

        void liveSystemPreferenceAndExplicitOverride()
        {
            preference("org.freedesktop.appearance", uint(0));
            preference("org.gnome.desktop.interface", "prefer-dark");
            window->findChild<QAction *>("appearance_system")->trigger();
            QVERIFY(ColorScheme::DetectDarkMode());
            preference("org.gnome.desktop.interface", "prefer-light");
            QVERIFY(!ColorScheme::DetectDarkMode());
            // Standard portal preference takes priority over the desktop fallback.
            preference("org.freedesktop.appearance", uint(1));
            QVERIFY(ColorScheme::DetectDarkMode());
            window->findChild<QAction *>("appearance_light")->trigger();
            preference("org.freedesktop.appearance", uint(1));
            QVERIFY(!ColorScheme::DetectDarkMode());
            window->findChild<QAction *>("appearance_system")->trigger();
            QVERIFY(ColorScheme::DetectDarkMode());
            preference("org.freedesktop.appearance", QVariant::fromValue(QDBusVariant(uint(2))));
            QVERIFY(!ColorScheme::DetectDarkMode());
            // With no preference, recover the native palette, not our last override.
            preference("org.freedesktop.appearance", uint(0));
            preference("org.gnome.desktop.interface", "default");
            QVERIFY(!ColorScheme::DetectDarkMode());
        }

        void customGraphColorsSurviveAppearanceSwitch()
        {
            CFG->UseCustomColorScheme = true;
            CFG->CustomColorScheme.insert("CpuGraphLineColor", QColor("#123456"));
            ThemeManager::Get()->SetMode("dark");
            QCOMPARE(ColorScheme::GetCurrent()->CpuGraphLineColor, QColor("#123456"));
            QVERIFY(ColorScheme::GetCurrent()->DarkMode);
            ThemeManager::Get()->SetMode("light");
            QCOMPARE(ColorScheme::GetCurrent()->CpuGraphLineColor, QColor("#123456"));
            QVERIFY(!ColorScheme::GetCurrent()->DarkMode);
            CFG->UseCustomColorScheme = false;
            ThemeManager::Get()->Apply();
        }

        void refreshMenuResumesEachTab()
        {
            QTabWidget *tabs = window->findChild<QTabWidget *>("tabWidget");
            QVERIFY(tabs);
            QVERIFY(interval(0));
            QVERIFY(interval(500));
            for (int index : { 0, 2, 3 })
            {
                tabs->setCurrentIndex(index);
                QWidget *owner = index == 0 ? static_cast<QWidget *>(window->findChild<ProcessesWidget *>())
                    : index == 2 ? static_cast<QWidget *>(window->findChild<UsersWidget *>())
                                 : static_cast<QWidget *>(window->findChild<ServicesWidget *>());
                QVERIFY(owner);
                const auto timers = owner->findChildren<QTimer *>(QString(), Qt::FindDirectChildrenOnly);
                QCOMPARE(timers.size(), 1);
                interval(0)->trigger();
                QVERIFY(CFG->RefreshPaused);
                QVERIFY(!timers.first()->isActive());
                interval(500)->trigger();
                QVERIFY(!CFG->RefreshPaused);
                QVERIFY(timers.first()->isActive());
                QCOMPARE(timers.first()->interval(), 500);
                QCOMPARE(QSettings().value("General/RefreshRateMs").toInt(), 500);
            }
            interval(0)->trigger();
        }

        void graphHistoryAndSmallWindow()
        {
            QMenu *menu = window->findChild<QMenu *>("graphHistoryMenu");
            QVERIFY(menu);
            menu->actions().last()->trigger();
            QCOMPARE(CFG->PerfGraphWindowSec, 900);
            QCOMPARE(QSettings().value("Performance/GraphWindowSec").toInt(), 900);
            window->findChild<QTabWidget *>("tabWidget")->setCurrentIndex(1);
            window->resize(640, 480);
            for (const QString &mode : { QString("dark"), QString("light") })
            {
                ThemeManager::Get()->SetMode(mode);
                QCoreApplication::processEvents();
                QVERIFY(window->width() <= 640);
                QVERIFY(window->height() <= 480);
                QVERIFY(window->grab().save("appearance-" + mode + ".png"));
            }
        }

        void cleanupTestCase()
        {
            window.reset();
        }
};

int main(int argc, char **argv)
{
    QTemporaryDir config;
    if (!config.isValid())
        return 1;
    qputenv("XDG_CONFIG_HOME", config.path().toUtf8());
    QApplication app(argc, argv);
    app.setOrganizationName("TuxManagerTests");
    app.setApplicationName("Appearance");
    app.setStyle("Fusion");
    app.setPalette(app.style()->standardPalette());
    AppearanceTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "appearance_test.moc"
