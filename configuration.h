#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include <QByteArray>
#include <QObject>
#include <QVector>

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

        // ── Processes tab ─────────────────────────────────────────────────────────
        bool ShowKernelTasks     { true };  ///< Show kernel threads in the process list
        bool ShowOtherUsersProcs { true };  ///< Show processes of other users
        int  ProcessListSortColumn { 4 };   ///< ColCpu — column index to sort by
        int  ProcessListSortOrder  { 1 };   ///< Qt::DescendingOrder

        // ── Performance tab (GPU) ─────────────────────────────────────────────
        /// Selected engine indices for the 4 GPU engine selectors.
        QVector<int> GpuEngineSelectorIndices { 0, 1, 2, 3 };
        /// CPU graph mode: 0 = overall, 1 = logical processors.
        int CpuGraphMode { 0 };
        /// CPU overlay toggle in Performance -> CPU.
        bool CpuShowKernelTimes { false };

    private:
        explicit Configuration(QObject *parent = nullptr);
        ~Configuration() override = default;

        static Configuration *s_instance;
};

#endif // CONFIGURATION_H
