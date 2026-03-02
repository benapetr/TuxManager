#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include <QByteArray>
#include <QObject>

// Convenience macro for global config access: CFG->SomeSetting
#define CFG (Configuration::instance())

class Configuration : public QObject
{
    Q_OBJECT

public:
    static Configuration *instance();

    /// Read all settings from the backing store into public members.
    void Load();

    /// Write all public members back to the backing store.
    void Save();

    // ── Window layout ────────────────────────────────────────────────────────
    QByteArray WindowGeometry;   ///< Saved via QMainWindow::saveGeometry()
    QByteArray WindowState;      ///< Saved via QMainWindow::saveState()
    int        ActiveTab { 0 };  ///< Index of the last active tab

    // ── General ──────────────────────────────────────────────────────────────
    int RefreshRateMs { 1000 };  ///< How often live data is refreshed (ms)

private:
    explicit Configuration(QObject *parent = nullptr);
    ~Configuration() override = default;

    static Configuration *s_instance;
};

#endif // CONFIGURATION_H
