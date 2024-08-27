#pragma once

#include <cstddef>
#include <memory>

namespace detail {
struct control_block {
  virtual void clear() = 0;
  virtual ~control_block();

  void check_lifetime();

  void inc_strong();

  void inc_weak();

  void dec_strong();

  void dec_weak();

  size_t get_str_ref_cnt();

private:
  size_t strong_ref_count{1};
  size_t weak_ref_count{};
};

template <typename T, typename D = std::default_delete<T>>
struct control_block_ptr : control_block {
public:
  control_block_ptr(T* _ptr, D&& del) : ptr(_ptr), deleter(std::move(del)) {}

  ~control_block_ptr() override = default;

private:
  void clear() override {
    deleter(ptr);
  }

  T* ptr;
  [[no_unique_address]] D deleter;
};

template <typename T>
struct control_block_obj : control_block {
  template <typename... Args>
  explicit control_block_obj(Args&&... args) {
    new (&data) T(std::forward<Args>(args)...);
  }

  T* get_ptr() {
    return std::launder(reinterpret_cast<T*>(data));
  }

  ~control_block_obj() override = default;

private:
  void clear() override {
    std::launder(get_ptr())->~T();
  }

  alignas(T) std::byte data[sizeof(T)];
};
} // namespace detail
