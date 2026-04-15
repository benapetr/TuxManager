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

#ifndef NETWORK_H
#define NETWORK_H

#include "../globals.h"
#include "../historybuffer.h"
#include <memory>
#include <QElapsedTimer>
#include <QStringList>
#include <QVector>
#include <vector>

class Network
{
    public:
        struct NetworkInfo
        {
            QString         Name;      ///< Interface name, e.g. enp5s0
            QString         Type;      ///< Ethernet/Wi-Fi/Other
            QString         IPv4;
            QStringList     IPv6;
            bool            IsActive { true };
            int             LinkSpeedMbps { 0 };
            quint64         PrevRxBytes { 0 };
            quint64         PrevTxBytes { 0 };
            double          RxBps { 0.0 };
            double          TxBps { 0.0 };
            double          MaxThroughputBps { 0.0 };
            HistoryBuffer   RxHistory { TUX_MANAGER_HISTORY_SIZE };
            HistoryBuffer   TxHistory { TUX_MANAGER_HISTORY_SIZE };
        };

        Network();

        /// Sample /proc/net/dev counters and compute per-interface RX/TX throughput histories.
        bool Sample();

        int NetworkCount() const { return static_cast<int>(this->m_networks.size()); }
        const NetworkInfo &FromIndex(int i) const;

    private:
        static bool isActiveNetworkInterface(const QString &name);
        static QString networkTypeFromArpType(int arpType);
        static int readLinkSpeedMbps(const QString &name);

        void refreshNetworkState(bool force = false);
        void refreshNetworkMetadata(bool force = false);

        std::vector<std::unique_ptr<NetworkInfo>>   m_networks;
        NetworkInfo            m_nullNetwork;
        QElapsedTimer          m_netTimer;
        qint64                 m_prevNetSampleMs { 0 };
        int                    m_networkStateRefreshCounter { 0 };
        int                    m_networkMetadataRefreshCounter { 0 };
};

#endif // NETWORK_H
