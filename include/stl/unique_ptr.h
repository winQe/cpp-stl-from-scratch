/**
 * @file unique_ptr.h
 * @brief Implementation of unique ptr
 */

#include <utility>

#pragma once
namespace stl {

template <typename T>
struct default_delete {
  constexpr default_delete() noexcept = default;
  // Pretty much does nothing because the struct is stateless, just for overload
  // resolution
  template <typename U>
  default_delete(const default_delete<U>&) noexcept {}

  void operator()(T* ptr) const {
    static_assert(sizeof(T) > 0, "Cannot delete incomplete type");
    delete ptr;
  }
};

template <typename T>
struct default_delete<T[]> {
  constexpr default_delete() noexcept = default;
  template <typename U>
  default_delete(const default_delete<U[]>&) noexcept {}

  void operator()(T* ptr) const {
    static_assert(sizeof(T) > 0, "Cannot delete incomplete type");
    delete[] ptr;
  }
};

template <typename T, typename Deleter = default_delete<T>>
class UniquePtr {
 public:
  // Constructors
  UniquePtr() noexcept : data_(nullptr), deleter_() {}
  explicit UniquePtr(T* raw_ptr) noexcept : data_(raw_ptr), deleter_() {}
  UniquePtr(T* raw_ptr, const Deleter& deleter) noexcept
      : data_(raw_ptr), deleter_(deleter) {}
  UniquePtr(T* raw_ptr, Deleter&& deleter) noexcept
      : data_(raw_ptr), deleter_(std::move(deleter)) {}

  ~UniquePtr() noexcept {
    if (data_) {
      deleter_(data_);
    }
  }

  // Template move constructor for compatible types
  template <typename U, typename D>
  UniquePtr(UniquePtr<U, D>&& other) noexcept
      : data_(other.release()), deleter_(std::move(other.get_deleter())) {}

  // By definition, unique ptr should disable copy and copy assignment
  // constructors. You cannot copy ownership
  UniquePtr(const UniquePtr& other) = delete;
  UniquePtr& operator=(const UniquePtr& other) = delete;

  UniquePtr(UniquePtr&& other) noexcept : data_(other.data_) {
    other.data_ = nullptr;
  }

  UniquePtr& operator=(UniquePtr&& other) noexcept {
    if (this != &other) {
      // Clean up current resource
      if (data_) {
        deleter_(data_);
      }

      data_ = other.data_;
      deleter_ = std::move(other.deleter_);
      other.data_ = nullptr;
    }
    return *this;
  }

  T* get() noexcept { return data_; }
  const T* get() const noexcept { return data_; }

  T& operator*() noexcept { return *data_; }
  const T& operator*() const noexcept { return *data_; }

  T* operator->() noexcept { return data_; }
  const T* operator->() const noexcept { return data_; }

  void reset(T* raw_ptr = nullptr) noexcept {
    if (data_) {
      deleter_(data_);
    }
    data_ = raw_ptr;
  }
  T* release() noexcept {
    auto ptr = data_;
    data_ = nullptr;

    return ptr;
  }

  explicit operator bool() const noexcept { return data_; }

 private:
  T* data_;
  Deleter deleter_;
};
}  // namespace stl
