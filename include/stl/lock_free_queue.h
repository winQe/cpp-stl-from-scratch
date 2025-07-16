/**
 * @file lock_free_queue.h
 * @brief Implementation of MPMC ring buffer based lock freee queue (Bounded
 * Vyukov Queue)
 */

#pragma once

#include <array>
#include <atomic>
#include <concepts>
#include <cstddef>
#include <utility>

constexpr uint kCacheLineSize = 64;

namespace stl {
template <typename T, size_t Capacity>
class LockFreeQueue {
  // Performance reasons -> modulo operation compiles to a single AND operation
  static_assert((Capacity & (Capacity - 1)) == 0,
                "Capacity must be power of 2");
  static constexpr size_t MASK = Capacity - 1;

 private:
  struct alignas(kCacheLineSize) Cell {
    std::atomic<size_t> seq;
    T data;
  };
  alignas(kCacheLineSize) std::array<Cell, Capacity> buffer_;
  alignas(kCacheLineSize) std::atomic<size_t> enqueue_index_ = 0;
  alignas(kCacheLineSize) std::atomic<size_t> dequeue_index_ = 0;

 public:
  LockFreeQueue() {
    for (size_t i = 0; i < Capacity; i++) {
      buffer_[i].seq.store(i, std::memory_order_relaxed);
    }
  }

  template <typename U>
    requires std::convertible_to<U&&, T>
  // Perfect forwarding: accepts both lvalues and rvalues
  bool try_push(U&& item) {
    Cell* cell = nullptr;

    size_t current_pos = enqueue_index_.load(std::memory_order_relaxed);

    for (;;) {
      cell = &buffer_[current_pos & MASK];
      size_t seq = cell->seq.load(std::memory_order_acquire);

      intptr_t diff = (intptr_t)seq - (intptr_t)(current_pos);

      // Cell ready for writing, no contention
      if (diff == 0) {
        if (enqueue_index_.compare_exchange_weak(current_pos, current_pos + 1,
                                                 std::memory_order_relaxed)) {
          // Claim the current pos and can write data here
          break;
        }
      } else if (diff < 0) {
        // Buffer full, seq is behind position
        return false;
      } else {  // dif > 0: Another thread is working on this cell
        // Reload position and try again
        current_pos = enqueue_index_.load(std::memory_order_relaxed);
      }
    }

    // Perfect forwarding
    cell->data = T(std::forward<U>(item));
    // Ready for consumer
    cell->seq.store(current_pos + 1, std::memory_order_release);

    return true;
  }

  bool try_pop(T& item) {
    Cell* cell = nullptr;

    size_t current_pos = dequeue_index_.load(std::memory_order_relaxed);

    for (;;) {
      cell = &buffer_[current_pos & MASK];
      size_t seq = cell->seq.load(std::memory_order_acquire);

      intptr_t diff = (intptr_t)seq - (intptr_t)(current_pos + 1);

      if (diff == 0) {
        // Ready to be read
        if (dequeue_index_.compare_exchange_weak(current_pos, current_pos + 1,
                                                 std::memory_order_relaxed)) {
          break;
        }
      } else if (diff < 0) {
        // Buffer is empty, no one touched it
        return false;
      } else {
        // Contention with another thread, they already started working on this
        current_pos = dequeue_index_.load(std::memory_order_relaxed);
      }
    }

    item = std::move(cell->data);
    // Increment the expected sequence by an entire cycle (capacity)
    cell->seq.store(current_pos + MASK + 1, std::memory_order_release);

    return true;
  }
};

}  // namespace stl
