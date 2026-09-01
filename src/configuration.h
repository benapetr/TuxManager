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

#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include <QByteArray>
#include <QObject>
#include <QStringList>
#include <QVariantMap>
#include <QVector>
#include <sys/types.h>

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
        bool RefreshPaused { false }; ///< True when periodic refresh is paused.
        bool UseCustomColorScheme { false };
        QVariantMap CustomColorScheme;
        bool IsSuperuser { false };  ///< True when effective UID is 0 (runtime-only).
        uid_t EUID { 0 };            ///< Effective user ID captured at startup (runtime-only).
        bool IOMetricsEnabled { false }; ///< True when any Processes I/O column is visible (runtime-only).
        bool ForceGpuDrm { false };  ///< Force DRM GPU backend and skip NVML detection (runtime-only).
        //! Intervals (in ms) available in menus for refresh rate (250ms, 1s, 5s etc.)
        QVector<int> RefreshRateAvailableIntervals;

        //! Intervals (in s) for how long should we retain the data (1 min, 2 min etc.)
        QVector<int> DataWindowAvailableIntervals;

        // ── Processes tab ─────────────────────────────────────────────────────────
        bool ShowKernelTasks     { true };  ///< Show kernel threads in the process list
        bool ShowOtherUsersProcs { true };  ///< Show processes of other users
        bool ProcessTreeView     { false }; ///< Processes tab: false=table, true=tree
        int  ProcessListSortColumn { 4 };   ///< ColCpu — column index to sort by
        int  ProcessListSortOrder  { 1 };   ///< Qt::DescendingOrder
        int  ProcessColumnSchemaVersion { 0 }; ///< Version for persisted process header states.
        QByteArray ProcessListHeaderState;  ///< Saved via QHeaderView::saveState()
        QByteArray ProcessTreeHeaderState;  ///< Saved via QHeaderView::saveState()
        QStringList TaskHistory;            ///< Most recently executed task commands.
        QString LastTaskDirectory;          ///< Last directory used in Run new task browse dialog.

        // ── Services tab ──────────────────────────────────────────────────────────
        QByteArray ServicesHeaderState;     ///< Saved via QHeaderView::saveState()

        // ── Performance tab (GPU) ─────────────────────────────────────────────
        /// Selected engine indices for the 4 GPU engine selectors.
        QVector<int> GpuEngineSelectorIndices { 0, 1, 2, 3 };
        /// CPU graph mode: 0 = overall, 1 = logical processors.
        int CpuGraphMode { 0 };
        /// Swap graph mode: 0 = overall, 1 = swap devices.
        int SwapGraphMode { 0 };
        /// CPU overlay toggle in Performance -> CPU.
        bool CpuShowKernelTimes { false };
        /// Performance selector visibility toggles.
        bool PerfShowCpu { true };
        bool PerfShowMemory { true };
        bool PerfShowSwap { true };
        bool PerfShowDisks { true };
        bool PerfShowNetwork { true };
        bool PerfShowGpu { true };
        bool SidePanelGridEnabled { false };
        bool PerfNetworkUseBits { true };
        int PerfGraphWindowSec { 60 };
        QStringList PerfSidePanelGroupOrder;
        QByteArray PerformanceSplitterState;   ///< Saved via QSplitter::saveState()

    private:
        explicit Configuration(QObject *parent = nullptr);
        ~Configuration() override = default;

        static Configuration *s_instance;
};

#endif // CONFIGURATION_H
