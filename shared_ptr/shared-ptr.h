#pragma once

#include "cb_details.h"

#include <cstddef>
#include <iostream>
#include <memory>

template <typename T>
class weak_ptr;

template <typename T>
class shared_ptr {
  template <typename>
  friend class shared_ptr;
  template <typename>
  friend class weak_ptr;
  template <typename T1, typename... Args>
  friend shared_ptr<T1> make_shared(Args&&... args);

  using control_block = detail::control_block;

public:
  shared_ptr() noexcept : cb(nullptr), ptr(nullptr) {}

  shared_ptr(std::nullptr_t) noexcept : cb(nullptr), ptr(nullptr) {}

  template <typename Y>
  explicit shared_ptr(Y* tempPtr) : ptr(tempPtr) {
    try {
      cb = new detail::control_block_ptr<Y>(tempPtr, std::default_delete<Y>());
    } catch (...) {
      delete ptr;
      throw;
    }
  }

  template <typename Y, typename Deleter>
  shared_ptr(Y* ptr, Deleter deleter) : ptr(ptr) {
    try {
      cb = new detail::control_block_ptr<Y, Deleter>(ptr, std::move(deleter));
    } catch (...) {
      deleter(ptr);
      throw;
    }
  }

  shared_ptr(const shared_ptr& other) noexcept : shared_ptr(other, other.get()) {}

  template <typename Y>
    requires(std::is_convertible_v<Y*, T*>)
  shared_ptr(const shared_ptr<Y>& other) noexcept : shared_ptr(other, other.get()) {}

  template <typename Y>
  shared_ptr(const shared_ptr<Y>& other, T* ptr) noexcept : cb(other.cb) {
    if (cb) {
      cb->inc_strong();
    }
    this->ptr = ptr;
  }

  template <typename Y>
  shared_ptr(shared_ptr<Y>&& other, T* ptr) noexcept : cb(std::move(other.cb)),
                                                       ptr(ptr) {
    other.set_nulls();
  }

  shared_ptr(shared_ptr&& other) noexcept : cb(nullptr), ptr(nullptr) {
    std::swap(cb, other.cb);
    std::swap(ptr, other.ptr);
  }

  template <typename Y>
    requires(std::is_convertible_v<Y*, T*>)
  shared_ptr(shared_ptr<Y>&& other) noexcept : shared_ptr(std::move(other), other.ptr) {}

  template <class Y>
  explicit shared_ptr(const weak_ptr<Y>& other) : cb(other.cb),
                                                  ptr(other.ptr) {
    if (cb) {
      cb->inc_strong();
    }
  }

  shared_ptr& operator=(const shared_ptr& other) noexcept {
    shared_ptr<T>(other).swap(*this);
    return *this;
  }

  template <typename Y>
  shared_ptr& operator=(const shared_ptr<Y>& other) noexcept {
    shared_ptr<T>(other).swap(*this);
    return *this;
  }

  shared_ptr& operator=(shared_ptr&& other) noexcept {
    shared_ptr<T>(std::move(other)).swap(*this);
    return *this;
  }

  template <typename Y>
  shared_ptr& operator=(shared_ptr<Y>&& other) noexcept {
    shared_ptr<T>(std::move(other)).swap(*this);
    return *this;
  }

  T* get() const noexcept {
    return ptr;
  }

  operator bool() const noexcept {
    return get() != nullptr;
  }

  T& operator*() const noexcept {
    return *get();
  }

  T* operator->() const noexcept {
    return get();
  }

  std::size_t use_count() const noexcept {
    return (cb == nullptr) ? 0 : cb->get_str_ref_cnt();
  }

  void reset() noexcept {
    shared_ptr().swap(*this);
  }

  template <typename Y>
  void reset(Y* new_ptr) {
    shared_ptr<T>(new_ptr).swap(*this);
  }

  template <typename Y, typename Deleter>
  void reset(Y* new_ptr, Deleter deleter) {
    shared_ptr<T>(new_ptr, std::move(deleter)).swap(*this);
  }

  friend bool operator==(const shared_ptr& lhs, const shared_ptr& rhs) noexcept {
    return lhs.get() == rhs.get();
  }

  friend bool operator!=(const shared_ptr& lhs, const shared_ptr& rhs) noexcept {
    return !(lhs == rhs);
  }

  ~shared_ptr() {
    if (cb) {
      cb->dec_strong();
    }
  }

  void swap(shared_ptr& other) noexcept {
    std::swap(ptr, other.ptr);
    std::swap(cb, other.cb);
  }

private:
  shared_ptr(detail::control_block_obj<T>* ptr) : cb(ptr), ptr(ptr->get_ptr()) {}

  void set_nulls() {
    ptr = nullptr;
    cb = nullptr;
  }

  control_block* cb;
  T* ptr;
};

template <typename T>
class weak_ptr {
  template <typename>
  friend class shared_ptr;
  template <typename>
  friend class weak_ptr;

  using control_block = detail::control_block;

public:
  weak_ptr() noexcept : cb(nullptr), ptr(nullptr) {}

  template <typename Y>
  weak_ptr(const shared_ptr<Y>& other) noexcept : cb(other.cb),
                                                  ptr(other.ptr) {
    if (cb) {
      cb->inc_weak();
    }
  }

  weak_ptr(const weak_ptr& other) noexcept : cb(other.cb), ptr(other.ptr) {
    if (cb) {
      cb->inc_weak();
    }
  }

  template <typename Y>
  weak_ptr(const weak_ptr<Y>& other) noexcept : cb(other.cb),
                                                ptr(other.ptr) {
    if (cb) {
      cb->inc_weak();
    }
  }

  weak_ptr(weak_ptr&& other) noexcept : cb(std::move(other.cb)), ptr(std::move(other.ptr)) {
    other.set_nulls();
  }

  template <typename Y>
  weak_ptr(weak_ptr<Y>&& other) noexcept : cb(std::move(other.cb)),
                                           ptr(std::move(other.ptr)) {
    other.set_nulls();
  }

  template <typename Y>
  weak_ptr& operator=(const shared_ptr<Y>& other) noexcept {
    weak_ptr<T>(other).swap(*this);
    return *this;
  }

  weak_ptr& operator=(const weak_ptr& other) noexcept {
    weak_ptr<T>(other).swap(*this);
    return *this;
  }

  template <typename Y>
  weak_ptr& operator=(const weak_ptr<Y>& other) noexcept {
    weak_ptr<T>(other).swap(*this);
    return *this;
  }

  weak_ptr& operator=(weak_ptr&& other) noexcept {
    weak_ptr<T>(std::move(other)).swap(*this);
    return *this;
  }

  template <typename Y>
  weak_ptr& operator=(weak_ptr<Y>&& other) noexcept {
    weak_ptr<T>(std::move(other)).swap(*this);
    return *this;
  }

  shared_ptr<T> lock() const noexcept {
    return (use_count() == 0) ? shared_ptr<T>() : shared_ptr<T>(*this);
  }

  std::size_t use_count() const noexcept {
    return (cb == nullptr) ? 0 : cb->get_str_ref_cnt();
  }

  void reset() noexcept {
    if (cb) {
      cb->dec_weak();
    }
    set_nulls();
  }

  void swap(weak_ptr& other) {
    std::swap(cb, other.cb);
    std::swap(ptr, other.ptr);
  }

  ~weak_ptr() {
    if (cb) {
      cb->dec_weak();
    }
  }

private:
  control_block* cb;
  T* ptr;

  void set_nulls() {
    ptr = nullptr;
    cb = nullptr;
  }
};

template <typename T, typename... Args>
shared_ptr<T> make_shared(Args&&... args) {
  return shared_ptr<T>(new detail::control_block_obj<T>(std::forward<Args>(args)...));
}
