/**
 * @file unique_ptr.h
 * @brief Implementation of unique ptr
 */

#pragma once
#include <type_traits>
#include <utility>

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
    requires std::is_convertible_v<U*, T*> && std::is_convertible_v<D, Deleter>
  UniquePtr(UniquePtr<U, D>&& other) noexcept
      : data_(other.release()), deleter_(std::move(other.get_deleter())) {}

  // By definition, unique ptr should disable copy and copy assignment
  // constructors. You cannot copy ownership
  UniquePtr(const UniquePtr& other) = delete;
  UniquePtr& operator=(const UniquePtr& other) = delete;

  UniquePtr(UniquePtr&& o) noexcept(
      std::is_nothrow_move_constructible_v<Deleter>)
      : data_{o.release()}, deleter_{std::move(o.deleter_)} {}

  UniquePtr& operator=(UniquePtr&& other) noexcept {
    if (this != &other) {
      // Clean up current resource
      reset(other.release());
      deleter_ = std::move(other.deleter_);
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
    // Guarding against the case where self reset
    if (data_ == raw_ptr) {
      return;
    }

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

  void swap(UniquePtr& other) noexcept {
    using std::swap;
    swap(data_, other.data_);
    swap(deleter_, other.deleter_);
  }
  Deleter& get_deleter() noexcept { return deleter_; }
  const Deleter& get_deleter() const noexcept { return deleter_; }

 private:
  T* data_;
  Deleter deleter_;
};

// Perfect forwarding
template <typename T, typename... Args>
  requires std::constructible_from<T, Args&&...>
UniquePtr<T> make_unique(Args&&... args) {
  return UniquePtr<T>(new T(std::forward<Args>(args)...));
}
}  // namespace stl
