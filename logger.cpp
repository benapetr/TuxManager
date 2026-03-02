#include "logger.h"

#include <QDateTime>
#include <QTextStream>

Logger *Logger::s_instance = nullptr;

Logger *Logger::instance()
{
    if (!s_instance)
        s_instance = new Logger();
    return s_instance;
}

Logger::Logger(QObject *parent) : QObject(parent)
{}

const char *Logger::levelTag(Level level)
{
    switch (level)
    {
        case Debug:
            return "DEBUG";
        case Info:
            return "INFO ";
        case Warn:
            return "WARN ";
        case Error:
            return "ERROR";
    }
    return "?????";
}

void Logger::Log(Level level, const QString &message)
{
    // Filter based on Verbosity:
    //   Verbosity 0 → show Info and above (suppress Debug)
    //   Verbosity 1+ → show everything
    const Level minLevel = (Verbosity >= 1) ? Debug : Info;
    if (level < minLevel)
        return;

    const QString timestamp =
        QDateTime::currentDateTime().toString("[yyyy-MM-dd HH:mm:ss.zzz]");
    const QString line =
        QString("%1 %2: %3\n").arg(timestamp, levelTag(level), message);

    // WARN and ERROR → stderr, everything else → stdout
    if (level >= Warn)
    {
        QTextStream err(stderr);
        err << line;
        err.flush();
    } else
    {
        QTextStream out(stdout);
        out << line;
        out.flush();
    }
}
