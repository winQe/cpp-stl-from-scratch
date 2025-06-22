/**
 * @file vector.h
 * @brief Implementation of resizing array
 */

#pragma once

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <cstdlib>
#include <utility>

namespace stl {

template <typename T>
class Vector {
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
    for (size_t i = 0; i < size_; ++i) data_[i].~T();
    free(data_);
  }

  Vector(const Vector &other)
      : data_(static_cast<T *>(malloc(sizeof(T) * other.capacity_))),
        capacity_(other.capacity_),
        size_(other.size_) {
    std::copy(other.data_, other.data_ + size_, data_);
  }

  void swap(Vector &other) noexcept {
    using std::swap;
    swap(size_, other.size_);
    swap(capacity_, other.capacity_);
    swap(data_, other.data_);
  }

  friend void swap(Vector &lhs, Vector &rhs) noexcept { lhs.swap(rhs); }

  Vector &operator=(const Vector &other) {
    Vector copy(other);
    copy.swap(*this);

    return *this;
  }

  Vector(Vector &&other) noexcept {
    capacity_ = std::exchange(other.capacity_, 0);
    size_ = std::exchange(other.size_, 0);
    data_ = std::exchange(other.data_, nullptr);
  }

  Vector &operator=(Vector &&other) noexcept {
    Vector copy(std::move(other));
    copy.swap(*this);

    return *this;
  }

  [[nodiscard]] size_t size() const noexcept { return size_; }
  [[nodiscard]] size_t capacity() const noexcept { return capacity_; }
  [[nodiscard]] bool empty() const noexcept { return size_ == 0; }

  // Forwarding reference to ensure our implementation accepts all value
  // categories
  template <typename U>
    requires std::convertible_to<U &&, T>
  void push_back(U &&element) {
    // Classic dynamic resizing array implementation
    // Exceed capacity, then just allocate twice as much
    if (size_ == capacity_) {
      reallocate(capacity_ == 0 ? 1 : 2 * capacity_);
    }

    // Perfect forwarding
    new (data_ + size_) T(std::forward<U>(element));
    size_++;
  }

  template <typename... Args>
    requires std::constructible_from<T, Args &&...>
  void emplace_back(Args &&...args) {
    if (size_ == capacity_) {
      reallocate(capacity_ == 0 ? 1 : 2 * capacity_);
    }

    new (data_ + size_) T(std::forward<Args>(args)...);

    size_++;
  }

  void pop_back() {
    if (size_ == 0) return;
    // Destruct last element
    data_[size_ - 1].~T();
    size_--;

    // Only reduce the size when current size is 1/4 of the capacity
    if (size_ <= capacity_ / 4) {
      reallocate(capacity_ / 2);
    }
  }

  T &operator[](size_t index) noexcept { return data_[index]; }
  const T &operator[](size_t index) const noexcept { return data_[index]; }

  T &front() noexcept { return data_[0]; }
  const T &front() const noexcept { return data_[0]; }
  T &back() noexcept { return data_[size_ - 1]; }
  const T &back() const noexcept { return data_[size_ - 1]; }
  T *data() noexcept { return data_; }
  const T *data() const noexcept { return data_; }

  using iterator = T *;
  using const_iterator = const T *;
  // Iterator support
  iterator begin() noexcept { return data_; }
  const_iterator begin() const noexcept { return data_; }
  iterator end() noexcept { return data_ + size_; }
  const_iterator end() const noexcept { return data_ + size_; }

  void clear() noexcept {
    for (size_t i = 0; i < size_; ++i) data_[i].~T();
    size_ = 0;
  }

 private:
  void reallocate(size_t new_capacity) {
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
}  // namespace stl
