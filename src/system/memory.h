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

#ifndef MEMORY_H
#define MEMORY_H

#include "../globals.h"
#include "../historybuffer.h"
#include <QElapsedTimer>

class Memory
{
    public:
        Memory();

        /// Sample /proc/meminfo and append memory history.
        bool Sample();

        qint64 MemTotalKb()   const { return this->m_memTotalKb;   }
        /// In-use RAM including compressed memory pools.
        qint64 MemUsedKb()    const { return this->m_memUsedKb;    }
        /// In-use RAM excluding compressed memory pools' physical RAM footprint.
        qint64 MemUsedNonCompressedKb() const { return this->m_memUsedNonCompressedKb; }
        qint64 MemAvailKb()   const { return this->m_memAvailKb;   }
        /// Truly free (MemFree from /proc/meminfo)
        qint64 MemFreeKb()    const { return this->m_memFreeKb;    }
        /// Page cache: Cached + SReclaimable - Shmem + Buffers
        qint64 MemCachedKb()  const { return this->m_memCachedKb;  }
        qint64 MemBuffersKb() const { return this->m_memBuffersKb; }
        /// Dirty pages pending write-back: Dirty + Writeback
        qint64 MemDirtyKb()   const { return this->m_memDirtyKb;   }
        /// Total uncompressed payload currently stored in zram devices.
        qint64 ZramCompressedKb() const { return this->m_zramCompressedKb; }
        /// Physical RAM currently consumed by zram devices.
        qint64 ZramMemUsedKb() const { return this->m_zramMemUsedKb; }
        bool HasZram() const { return this->m_hasZram; }
        /// Total uncompressed payload currently stored in compressed memory pools.
        qint64 CompressedPayloadKb() const { return this->m_compressedPayloadKb; }
        /// Physical RAM currently consumed by compressed memory pools.
        qint64 CompressedRamUsedKb() const { return this->m_compressedRamUsedKb; }
        bool HasCompressedMemory() const { return this->m_hasCompressedMemory; }
        int MemDimmSlotsTotal() const { return this->m_memDimmSlotsTotal; }
        int MemDimmSlotsUsed()  const { return this->m_memDimmSlotsUsed;  }
        int MemSpeedMtps()      const { return this->m_memSpeedMtps;      }
        double MemFraction()  const;
        const HistoryBuffer &MemHistory() const { return this->m_memHistory; }

    private:
        void readHardwareMetadata();
        void updateZswapEnabled();

        qint64           m_memTotalKb   { 0 };
        qint64           m_memUsedKb    { 0 };
        qint64           m_memUsedNonCompressedKb { 0 };
        qint64           m_memAvailKb   { 0 };
        qint64           m_memFreeKb    { 0 };
        qint64           m_memCachedKb  { 0 };
        qint64           m_memBuffersKb { 0 };
        qint64           m_memDirtyKb   { 0 };
        qint64           m_zramCompressedKb { 0 };
        qint64           m_zramMemUsedKb { 0 };
        bool             m_hasZram { false };
        qint64           m_zswapPayloadKb { 0 };
        qint64           m_zswapMemUsedKb { 0 };
        bool             m_zswapEnabled { false };
        QElapsedTimer    m_zswapEnabledCheckTimer;
        qint64           m_compressedPayloadKb { 0 };
        qint64           m_compressedRamUsedKb { 0 };
        bool             m_hasCompressedMemory { false };
        int              m_memDimmSlotsTotal { 0 };
        int              m_memDimmSlotsUsed  { 0 };
        int              m_memSpeedMtps      { 0 };
        HistoryBuffer    m_memHistory { TUX_MANAGER_HISTORY_SIZE };
};

#endif // MEMORY_H
