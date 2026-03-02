#include "mainwindow.h"
#include "configuration.h"
#include "logger.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <cstdio>
#include <cstring>

static constexpr char kVersion[] = "0.1.0";
static constexpr char kAppName[] = "SystemInfo";

// Pre-screen --help / --version before QApplication so they work without a display.
static void printVersion()
{
    printf("%s %s\n", kAppName, kVersion);
}

static void printHelp()
{
    printf("Usage: %s [options]\n\n"
           "Linux system monitor inspired by Windows Task Manager.\n\n"
           "Options:\n"
           "  -h, --help     Show this help and exit\n"
           "  -V, --version  Show version and exit\n"
           "  -v             Increase log verbosity.\n"
           "                 Repeat for more detail: -v, -vv, -vvv\n",
           kAppName);
}

int main(int argc, char *argv[])
{
    // Handle --help / --version before constructing QApplication so they
    // are usable from a terminal without a display server present.
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--help") == 0 ||
            strcmp(argv[i], "-h")     == 0 ||
            strcmp(argv[i], "-?")     == 0) {
            printHelp();
            return 0;
        }
        if (strcmp(argv[i], "--version") == 0 ||
            strcmp(argv[i], "-V")        == 0) {
            printVersion();
            return 0;
        }
    }

    QApplication a(argc, argv);
    a.setOrganizationName(kAppName);
    a.setApplicationName(kAppName);
    a.setApplicationVersion(kVersion);

    // ── Command-line parsing (remaining options) ──────────────────────────────
    QCommandLineParser parser;
    parser.setApplicationDescription("Linux system monitor inspired by Windows Task Manager.");
    parser.addHelpOption();

    QCommandLineOption versionOption(
        QStringList{"V", "version"},
        "Display version information and exit.");
    parser.addOption(versionOption);

    QCommandLineOption verboseOption(
        QStringList{"v"},
        "Increase verbosity. Repeat for more detail: -v, -vv, -vvv.");
    parser.addOption(verboseOption);

    parser.process(a);

    // Count -v occurrences manually: handles -v, -vv, -vvv and mixed forms.
    // QCommandLineParser doesn't expose a count() in all Qt versions.
    int verbosity = 0;
    for (const QString &arg : a.arguments().mid(1)) {
        if (arg.startsWith("--")) continue;
        if (arg.startsWith('-')) {
            for (const QChar c : arg.mid(1))
                if (c == 'v') verbosity++;
        }
    }
    Logger::instance()->verbosity = verbosity;

    // ── Bootstrap ─────────────────────────────────────────────────────────────
    CFG->Load();
    LOG_INFO(QString("%1 %2 starting (verbosity=%3)")
             .arg(a.applicationName(), a.applicationVersion())
             .arg(verbosity));

    MainWindow w;
    w.show();
    return a.exec();
}
