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

#include "network.h"
#include "misc.h"
#include <QtCore/qobject.h>
#include <QHash>
#include <QIODevice>
#include <QFile>
#include <QDir>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <ifaddrs.h>

Network::Network() {}

const Network::NetworkInfo &Network::FromIndex(int i) const
{
    if (i < 0 || i >= static_cast<int>(this->m_networks.size()))
        return this->m_nullNetwork;
    return *this->m_networks.at(i);
}

bool Network::isActiveNetworkInterface(const QString &name)
{
    if (name.isEmpty() || name == "lo")
        return false;

    const QString operState = Misc::ReadFile(QString("/sys/class/net/%1/operstate").arg(name)).toLower();
    if (operState != "up")
        return false;

    const QString carrierStr = Misc::ReadFile(QString("/sys/class/net/%1/carrier").arg(name));
    if (!carrierStr.isEmpty() && carrierStr != "1")
        return false;

    return true;
}

QString Network::networkTypeFromArpType(int arpType)
{
    // ARPHRD_ETHER (1), ARPHRD_LOOPBACK (772), ARPHRD_IEEE80211* (801+)
    if (arpType == 1)
        return "Ethernet";
    if (arpType == 772)
        return "Loopback";
    if (arpType >= 801 && arpType <= 804)
        return "Wi-Fi";
    return QObject::tr("Network");
}

int Network::readLinkSpeedMbps(const QString &name)
{
    bool ok = false;
    const int speed = Misc::ReadFile(QString("/sys/class/net/%1/speed").arg(name)).toInt(&ok);
    if (!ok || speed <= 0)
        return 0;
    return speed;
}

void Network::refreshNetworkState(bool force)
{
    if (!force)
    {
        ++this->m_networkStateRefreshCounter;
        if (this->m_networkStateRefreshCounter < 10)
            return;
    }

    this->m_networkStateRefreshCounter = 0;
    for (const auto &n : this->m_networks)
        n->IsActive = isActiveNetworkInterface(n->Name);
}

void Network::refreshNetworkMetadata(bool force)
{
    if (!force)
    {
        ++this->m_networkMetadataRefreshCounter;
        if (this->m_networkMetadataRefreshCounter < 60)
            return;
    }

    this->m_networkMetadataRefreshCounter = 0;

    struct IfAddrInfo
    {
        QString ipv4;
        QStringList ipv6;
    };
    QHash<QString, IfAddrInfo> ifaddrByName;
    struct ifaddrs *ifaddr = nullptr;
    if (::getifaddrs(&ifaddr) == 0)
    {
        for (struct ifaddrs *ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next)
        {
            if (!ifa->ifa_name || !ifa->ifa_addr)
                continue;
            const QString name = QString::fromUtf8(ifa->ifa_name);

            char host[NI_MAXHOST] = {};
            const int fam = ifa->ifa_addr->sa_family;
            if (fam != AF_INET && fam != AF_INET6)
                continue;

            const socklen_t addrLen = (fam == AF_INET)
                                          ? static_cast<socklen_t>(sizeof(sockaddr_in))
                                          : static_cast<socklen_t>(sizeof(sockaddr_in6));
            if (::getnameinfo(ifa->ifa_addr, addrLen,
                              host, sizeof(host),
                              nullptr, 0,
                              NI_NUMERICHOST) != 0)
            {
                continue;
            }

            IfAddrInfo &info = ifaddrByName[name];
            if (fam == AF_INET && info.ipv4.isEmpty())
                info.ipv4 = QString::fromLatin1(host);
            else if (fam == AF_INET6)
            {
                const QString ipv6 = QString::fromLatin1(host);
                if (!info.ipv6.contains(ipv6))
                    info.ipv6.append(ipv6);
            }
        }
        ::freeifaddrs(ifaddr);
    }

    for (const auto &n : this->m_networks)
    {
        const int arpType = Misc::ReadFile(QString("/sys/class/net/%1/type").arg(n->Name)).toInt();
        n->Type = networkTypeFromArpType(arpType);
        n->LinkSpeedMbps = readLinkSpeedMbps(n->Name);
        n->IPv4 = ifaddrByName.value(n->Name).ipv4;
        n->IPv6 = ifaddrByName.value(n->Name).ipv6;
    }
}

bool Network::Sample()
{
    QFile f("/proc/net/dev");
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;

    struct NetCounters
    {
        quint64 rxBytes { 0 };
        quint64 txBytes { 0 };
    };
    QHash<QString, NetCounters> countersByName;
    QStringList seenNames;

    int lineNo = 0;
    for (;;)
    {
        const QByteArray line = f.readLine();
        if (line.isNull())
            break;
        ++lineNo;
        if (lineNo <= 2)
            continue; // headers

        const int colon = line.indexOf(':');
        if (colon < 0)
            continue;

        const QString ifName = QString::fromUtf8(line.left(colon)).trimmed();
        if (ifName.isEmpty() || ifName == QLatin1String("lo"))
            continue;

        const QList<QByteArray> fields = line.mid(colon + 1).simplified().split(' ');
        if (fields.size() < 9)
            continue;

        NetCounters c;
        c.rxBytes = fields.at(0).toULongLong();
        c.txBytes = fields.at(8).toULongLong();
        countersByName.insert(ifName, c);
        seenNames.append(ifName);
    }
    f.close();

    std::sort(seenNames.begin(), seenNames.end());

    const bool initializingNetworks = this->m_networks.empty();
    if (initializingNetworks)
    {
        this->m_networks.reserve(seenNames.size());
        for (const QString &name : seenNames)
        {
            if (!isActiveNetworkInterface(name))
                continue;

            auto n = std::make_unique<NetworkInfo>();
            n->Name = name;
            n->IsActive = true;
            this->m_networks.push_back(std::move(n));
        }
    }

    bool topologyChanged = false;
    QSet<QString> seenNameSet(seenNames.cbegin(), seenNames.cend());
    for (const auto &n : std::as_const(this->m_networks))
    {
        if (!seenNameSet.contains(n->Name))
        {
            topologyChanged = true;
            break;
        }
    }

    this->refreshNetworkState(initializingNetworks || topologyChanged);
    this->refreshNetworkMetadata(initializingNetworks || topologyChanged);

    if (!this->m_netTimer.isValid())
        this->m_netTimer.start();

    const qint64 nowMs = this->m_netTimer.elapsed();
    const qint64 dtMs = (this->m_prevNetSampleMs > 0) ? (nowMs - this->m_prevNetSampleMs) : 0;
    this->m_prevNetSampleMs = nowMs;

    for (const auto &n : this->m_networks)
    {
        const auto it = countersByName.constFind(n->Name);
        if (!n->IsActive || it == countersByName.cend())
        {
            n->RxBps = 0.0;
            n->TxBps = 0.0;
            Misc::PushHistoryAndUpdateMax(n->RxHistory, 0.0, n->MaxThroughputBps, TUX_MANAGER_MIN_RATE);
            Misc::PushHistoryAndUpdateMax(n->TxHistory, 0.0, n->MaxThroughputBps, TUX_MANAGER_MIN_RATE);
            continue;
        }

        const NetCounters c = it.value();
        if (dtMs <= 0)
        {
            n->PrevRxBytes = c.rxBytes;
            n->PrevTxBytes = c.txBytes;
            Misc::PushHistoryAndUpdateMax(n->RxHistory, 0.0, n->MaxThroughputBps, TUX_MANAGER_MIN_RATE);
            Misc::PushHistoryAndUpdateMax(n->TxHistory, 0.0, n->MaxThroughputBps, TUX_MANAGER_MIN_RATE);
            continue;
        }

        const quint64 dRx = (c.rxBytes >= n->PrevRxBytes) ? (c.rxBytes - n->PrevRxBytes) : 0;
        const quint64 dTx = (c.txBytes >= n->PrevTxBytes) ? (c.txBytes - n->PrevTxBytes) : 0;
        n->PrevRxBytes = c.rxBytes;
        n->PrevTxBytes = c.txBytes;

        n->RxBps = static_cast<double>(dRx) * 1000.0 / static_cast<double>(dtMs);
        n->TxBps = static_cast<double>(dTx) * 1000.0 / static_cast<double>(dtMs);
        Misc::PushHistoryAndUpdateMax(n->RxHistory, n->RxBps, n->MaxThroughputBps, TUX_MANAGER_MIN_RATE);
        Misc::PushHistoryAndUpdateMax(n->TxHistory, n->TxBps, n->MaxThroughputBps, TUX_MANAGER_MIN_RATE);
    }

    return true;
}
