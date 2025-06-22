/**
 * @file vector.h
 * @brief Implementation of resizing array
 */

#pragma once

#include <cstddef>
#include <cstdlib>
#include <utility>

namespace stl {

template <typename T> class Vector {
public:
  Vector(size_t capacity = 0) : capacity_(capacity) {
    if (capacity_ == 0) {
      data_ = nullptr;
    } else {
      data_ = static_cast<T *>(malloc(sizeof(T) * capacity));
    }
  }

  // RAII
  ~Vector() noexcept {
    for (size_t i = 0; i < size_; ++i)
      data_[i].~T();
    free(data_);
  }

  size_t size() const noexcept { return size_; }
  size_t capacity() const noexcept { return capacity_; }

private:
  void Reallocate(size_t new_capacity) {
    // Allocate new memory
    T *new_data = static_cast<T *>(malloc(sizeof(T) * new_capacity));

    // Copy the old data with placement new
    // The purpose is to avoid constructing objects we don't need (what happens
    // if we just do something like T new_data = new T[capacity_])
    //
    // We already allocated memory, just need to move them in place
    for (size_t i = 0; i < size_; i++) {
      new (new_data + i) T(std::move(data_[i]));
      // We need to destruct manually
      data_[i].~T();
    }

    // Free the old memory
    free(data_);

    data_ = new_data;
    capacity_ = new_capacity;
  }

  T *data_;
  size_t size_ = 0;
  size_t capacity_ = 0;
};
} // namespace stl
