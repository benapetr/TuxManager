#ifndef LOGGER_H
#define LOGGER_H

#include <QObject>
#include <QString>

// ── Convenience macros ───────────────────────────────────────────────────────
#define LOG_DEBUG(msg) Logger::instance()->Log(Logger::Debug, QString(msg))
#define LOG_INFO(msg)  Logger::instance()->Log(Logger::Info,  QString(msg))
#define LOG_WARN(msg)  Logger::instance()->log(Logger::Warn,  QString(msg))
#define LOG_ERROR(msg) Logger::instance()->Log(Logger::Error, QString(msg))

class Logger : public QObject
{
    Q_OBJECT

    public:
        enum Level { Debug = 0, Info, Warn, Error };

        static Logger *instance();

        /// Log a message at the given level.
        void Log(Level level, const QString &message);

        /// Verbosity count (number of -v flags supplied on the command line).
        ///   0 → show INFO, WARN, ERROR  (default)
        ///   1 → also show DEBUG         (-v)
        ///   2+→ reserved for future TRACE levels (-vv / -vvv)
        int Verbosity { 0 };

    private:
        explicit Logger(QObject *parent = nullptr);
        ~Logger() override = default;

        static Logger *s_instance;

        static const char *levelTag(Level level);
};

#endif // LOGGER_H
