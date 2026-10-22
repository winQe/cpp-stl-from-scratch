/**
 * @file lock_free_queue.h
 * @brief Implementation of MPMC ring buffer based lock free queue (Bounded
 * Vyukov Queue)
 *
 * T must be default-constructible and move-assignable, since every cell holds
 * a T and values get assigned into it.
 */

#pragma once

#include <array>
#include <atomic>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

namespace stl {

// Not using std::hardware_destructive_interference_size, it varies by
// compiler (Apple clang gives 256 on arm64)
inline constexpr std::size_t kCacheLineSize = 64;

template <typename T, std::size_t Capacity>
class LockFreeQueue {
  static_assert(Capacity > 0 && (Capacity & (Capacity - 1)) == 0,
                "Capacity must be a power of 2");
  static_assert(std::is_default_constructible_v<T>,
                "T must be default-constructible (cells hold a T)");
  static_assert(std::is_move_assignable_v<T>,
                "T must be move-assignable (values are assigned into cells)");

  // Performance reasons -> modulo operation compiles to a single AND operation
  static constexpr std::size_t MASK = Capacity - 1;

 private:
  // One cache line per cell to avoid false sharing
  struct alignas(kCacheLineSize) Cell {
    std::atomic<std::size_t> seq;
    T data;
  };
  alignas(kCacheLineSize) std::array<Cell, Capacity> buffer_;
  alignas(kCacheLineSize) std::atomic<std::size_t> enqueue_index_ = 0;
  alignas(kCacheLineSize) std::atomic<std::size_t> dequeue_index_ = 0;

 public:
  LockFreeQueue() {
    for (std::size_t i = 0; i < Capacity; i++) {
      buffer_[i].seq.store(i, std::memory_order_relaxed);
    }
  }

  template <typename U>
    requires std::convertible_to<U&&, T>
  // Perfect forwarding: accepts both lvalues and rvalues
  bool try_push(U&& item) {
    Cell* cell = nullptr;

    std::size_t current_pos = enqueue_index_.load(std::memory_order_relaxed);

    for (;;) {
      cell = &buffer_[current_pos & MASK];
      std::size_t seq = cell->seq.load(std::memory_order_acquire);

      auto diff = static_cast<std::intptr_t>(seq - current_pos);

      // Cell ready for writing, no contention
      if (diff == 0) {
        // Relaxed is fine, seq does the synchronization
        if (enqueue_index_.compare_exchange_weak(current_pos, current_pos + 1,
                                                 std::memory_order_relaxed)) {
          // Claim the current pos and can write data here
          break;
        }
      } else if (diff < 0) {
        // Buffer full, seq is behind position
        return false;
      } else {  // diff > 0: Another thread is working on this cell
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

    std::size_t current_pos = dequeue_index_.load(std::memory_order_relaxed);

    for (;;) {
      cell = &buffer_[current_pos & MASK];
      std::size_t seq = cell->seq.load(std::memory_order_acquire);

      auto diff = static_cast<std::intptr_t>(seq - (current_pos + 1));

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
