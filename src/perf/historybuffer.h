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

#ifndef PERF_HISTORYBUFFER_H
#define PERF_HISTORYBUFFER_H

#include <iterator>
#include <QVector>
#include <QtGlobal>

namespace Perf
{
    /**
     * Fixed-capacity history ring buffer for sampled performance metrics.
     *
     * New samples overwrite the oldest ones once the buffer is full. Callers
     * access values by logical index, where 0 is the oldest sample currently
     * retained and size()-1 is the newest.
     */
    class HistoryBuffer
    {
        public:
            class const_iterator
            {
                public:
                    using iterator_category = std::forward_iterator_tag;
                    using value_type = double;
                    using difference_type = int;
                    using pointer = const double *;
                    using reference = double;

                    const_iterator(const HistoryBuffer *buffer, int index)
                        : m_buffer(buffer), m_index(index)
                    {
                    }

                    double operator*() const
                    {
                        return this->m_buffer->At(this->m_index);
                    }

                    const_iterator &operator++()
                    {
                        ++this->m_index;
                        return *this;
                    }

                    bool operator==(const const_iterator &other) const
                    {
                        return this->m_buffer == other.m_buffer && this->m_index == other.m_index;
                    }

                    bool operator!=(const const_iterator &other) const
                    {
                        return !(*this == other);
                    }

                private:
                    const HistoryBuffer *m_buffer { nullptr };
                    int                  m_index { 0 };
            };

            explicit HistoryBuffer(int capacity = 0)
            {
                this->SetCapacity(capacity);
            }

            void SetCapacity(int capacity)
            {
                this->m_capacity = qMax(0, capacity);
                this->m_data.resize(this->m_capacity);
                this->Clear();
            }

            int Capacity() const
            {
                return this->m_capacity;
            }

            int capacity() const
            {
                return this->Capacity();
            }

            int Size() const
            {
                return this->m_size;
            }

            int size() const
            {
                return this->Size();
            }

            bool IsEmpty() const
            {
                return this->m_size == 0;
            }

            bool isEmpty() const
            {
                return this->IsEmpty();
            }

            bool IsFull() const
            {
                return this->m_size == this->m_capacity;
            }

            void Clear()
            {
                this->m_head = 0;
                this->m_size = 0;
            }

            void Push(double value)
            {
                if (this->m_capacity <= 0)
                    return;

                if (this->m_size < this->m_capacity)
                {
                    const int tail = (this->m_head + this->m_size) % this->m_capacity;
                    this->m_data[tail] = value;
                    ++this->m_size;
                    return;
                }

                this->m_data[this->m_head] = value;
                this->m_head = (this->m_head + 1) % this->m_capacity;
            }

            void append(double value)
            {
                this->Push(value);
            }

            double Front() const
            {
                return this->At(0);
            }

            double first() const
            {
                return this->Front();
            }

            double Back() const
            {
                return this->At(this->m_size - 1);
            }

            double last() const
            {
                return this->Back();
            }

            double At(int index) const
            {
                Q_ASSERT(index >= 0);
                Q_ASSERT(index < this->m_size);
                return this->m_data[this->physicalIndex(index)];
            }

            double at(int index) const
            {
                return this->At(index);
            }

            double operator[](int index) const
            {
                return this->At(index);
            }

            const_iterator begin() const
            {
                return const_iterator(this, 0);
            }

            const_iterator end() const
            {
                return const_iterator(this, this->m_size);
            }

        private:
            int physicalIndex(int logicalIndex) const
            {
                return (this->m_head + logicalIndex) % this->m_capacity;
            }

            QVector<double> m_data;
            int             m_capacity { 0 };
            int             m_head { 0 };
            int             m_size { 0 };
    };
} // namespace Perf

#endif // PERF_HISTORYBUFFER_H
