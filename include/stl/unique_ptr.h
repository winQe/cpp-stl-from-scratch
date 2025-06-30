/**
 * @file unique_ptr.h
 * @brief Implementation of unique ptr
 */

#pragma once
namespace stl {

template <typename T>
struct default_delete {
  constexpr default_delete() noexcept = default;

  void operator()(T* ptr) const {
    static_assert(sizeof(T) > 0, "Cannot delete incomplete type");
    delete ptr;
  }
};

template <typename T>
struct default_delete<T[]> {
  constexpr default_delete() noexcept = default;

  void operator()(T* ptr) const {
    static_assert(sizeof(T) > 0, "Cannot delete incomplete type");
    delete[] ptr;
  }
};

template <typename T, typename Deleter = default_delete<T>>
class UniquePtr {
 public:
  UniquePtr() noexcept : data_(nullptr), deleter_() {}

  explicit UniquePtr(T* raw_ptr) : data_(raw_ptr) {}
  ~UniquePtr() noexcept { Deleter(data_); }

  // By definition, unique ptr should disable copy and copy assignment
  // constructors. You cannot copy ownership
  UniquePtr(const UniquePtr& other) = delete;
  UniquePtr& operator=(const UniquePtr& other) = delete;

  UniquePtr(UniquePtr&& other) noexcept : data_(other.data_) {
    other.data_ = nullptr;
  }
  UniquePtr& operator=(UniquePtr&& other) noexcept {
    data_ = other.data_;
    other.data_ = nullptr;
    return *this;
  }

  T* get() noexcept { return data_; }
  const T* get() const noexcept { return data_; }

  T& operator*() noexcept { return *data_; }
  const T& operator*() const noexcept { return *data_; }

  T* operator->() noexcept { return data_; }
  const T* operator->() const noexcept { return data_; }

  void reset(T* raw_ptr = nullptr) noexcept { data_ = raw_ptr; }
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
