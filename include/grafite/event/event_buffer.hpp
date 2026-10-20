// =============================================================================
// GRAFITE - General-purpose Real-time Audio Framework
//
// A modular C++ framework for real-time and offline audio/DSP processing.
// GRAFITE is designed as a flexible foundation for building modular,
// reusable, and interoperable audio applications and processing components.
//
// Copyright (c) 2026 GRAFITE contributors.
//
// SPDX-License-Identifier: LGPL-3.0-or-later
//
// This file is part of GRAFITE and is distributed under the terms of the
// GNU Lesser General Public License. See the LICENSE file for details.
// =============================================================================

#ifndef GRAFITE_EVENT_EVENT_BUFFER_HPP
#define GRAFITE_EVENT_EVENT_BUFFER_HPP

#include <cstddef>
#include <memory>

#include "grafite/event/event.hpp"

namespace grafite::event {

/// Non-owning mutable view over event storage.
///
/// A view can append events up to its configured capacity without allocating
/// memory. The referenced storage and size counter must remain valid for the
/// lifetime of the view.
class EventBufferView {
public:
    EventBufferView() = default;

    EventBufferView(
        Event* data,
        std::size_t* size,
        std::size_t capacity
    ) noexcept
        : data_(data),
          size_(size),
          capacity_(capacity)
    {
    }

    /// Appends an event without allocating memory.
    ///
    /// Returns false when the available capacity has been exhausted.
    [[nodiscard]]
    bool push(const Event& event) noexcept
    {
        if (size_ == nullptr || *size_ >= capacity_) {
            return false;
        }

        data_[*size_] = event;
        ++(*size_);

        return true;
    }

    /// Removes all events while retaining the underlying storage.
    void clear() noexcept
    {
        if (size_ != nullptr) {
            *size_ = 0;
        }
    }

    [[nodiscard]]
    std::size_t size() const noexcept
    {
        return size_ == nullptr ? 0 : *size_;
    }

    [[nodiscard]]
    std::size_t capacity() const noexcept
    {
        return capacity_;
    }

    [[nodiscard]]
    bool empty() const noexcept
    {
        return size() == 0;
    }

    [[nodiscard]]
    const Event& operator[](std::size_t index) const noexcept
    {
        return data_[index];
    }

    [[nodiscard]]
    const Event* begin() const noexcept
    {
        return data_;
    }

    [[nodiscard]]
    const Event* end() const noexcept
    {
        return data_ + size();
    }

private:
    Event* data_ = nullptr;
    std::size_t* size_ = nullptr;
    std::size_t capacity_ = 0;
};


/// Non-owning read-only view over an event sequence.
class ConstEventBufferView {
public:
    ConstEventBufferView() = default;

    ConstEventBufferView(
        const Event* data,
        std::size_t size
    ) noexcept
        : data_(data),
          size_(size)
    {
    }

    [[nodiscard]]
    std::size_t size() const noexcept
    {
        return size_;
    }

    [[nodiscard]]
    bool empty() const noexcept
    {
        return size_ == 0;
    }

    [[nodiscard]]
    const Event& operator[](std::size_t index) const noexcept
    {
        return data_[index];
    }

    [[nodiscard]]
    const Event* begin() const noexcept
    {
        return data_;
    }

    [[nodiscard]]
    const Event* end() const noexcept
    {
        return data_ + size_;
    }

private:
    const Event* data_ = nullptr;
    std::size_t size_ = 0;
};


/// Owning fixed-capacity storage for real-time events.
///
/// Storage allocation is performed explicitly with resize(). Once allocated,
/// events can be appended and cleared without further memory allocation.
class EventBuffer {
public:
    EventBuffer() = default;

    explicit EventBuffer(std::size_t capacity)
    {
        resize(capacity);
    }

    /// Allocates storage for the requested event capacity.
    ///
    /// Existing events are discarded. This operation may allocate memory and
    /// is therefore not real-time safe.
    void resize(std::size_t capacity)
    {
        if (capacity == 0) {
            data_.reset();
        } else {
            data_ = std::make_unique<Event[]>(capacity);
        }

        size_ = 0;
        capacity_ = capacity;
    }

    /// Appends an event without allocating memory.
    ///
    /// Returns false when the configured capacity has been exhausted.
    [[nodiscard]]
    bool push(const Event& event) noexcept
    {
        return view().push(event);
    }

    /// Removes all events while retaining allocated storage.
    void clear() noexcept
    {
        size_ = 0;
    }

    [[nodiscard]]
    std::size_t size() const noexcept
    {
        return size_;
    }

    [[nodiscard]]
    std::size_t capacity() const noexcept
    {
        return capacity_;
    }

    [[nodiscard]]
    bool empty() const noexcept
    {
        return size_ == 0;
    }

    [[nodiscard]]
    const Event& operator[](std::size_t index) const noexcept
    {
        return data_[index];
    }

    [[nodiscard]]
    EventBufferView view() noexcept
    {
        return {
            data_.get(),
            &size_,
            capacity_
        };
    }

    [[nodiscard]]
    ConstEventBufferView view() const noexcept
    {
        return {
            data_.get(),
            size_
        };
    }

private:
    std::unique_ptr<Event[]> data_;
    std::size_t size_ = 0;
    std::size_t capacity_ = 0;
};

}  // namespace grafite::event

#endif  // GRAFITE_EVENT_EVENT_BUFFER_HPP