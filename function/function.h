#pragma once
#include <cstddef>
#include <exception>
#include <new>
#include <type_traits>
#include <utility>

class bad_function_call : public std::exception {
  const char* what() const noexcept override {
    return "Called operator() from empty function";
  }
};

namespace details {
struct empty {};

using data_t = std::aligned_storage_t<sizeof(std::max_align_t), alignof(std::max_align_t)>;
template <typename F>
static constexpr bool is_small = (sizeof(F) <= sizeof(data_t)) && std::is_nothrow_move_constructible_v<F>;

template <typename R, typename... Args>
struct base_descriptor {
  virtual R invoke_fn([[maybe_unused]] data_t* fn, [[maybe_unused]] Args... args) const = 0;

  virtual void copy([[maybe_unused]] data_t* src, [[maybe_unused]] data_t* dst) const {}

  virtual void destroy_fn([[maybe_unused]] data_t* fn) const noexcept {}

  virtual void move_fn([[maybe_unused]] data_t* src, [[maybe_unused]] data_t* dst) const noexcept {}
};

template <typename F, typename R, typename... Args>
struct descriptor : base_descriptor<R, Args...> {
  static F& cast(data_t* obj) noexcept {
    return *std::launder(reinterpret_cast<F*>(obj));
  }

  static const F& cast_const(data_t* obj) noexcept {
    return *std::launder(reinterpret_cast<const F*>(obj));
  }

  static F* get_pointer(data_t& data) {
    return &cast(&data);
  }

  static void fill_data(data_t& dst, F&& func) {
    new (&dst) F(std::move(func));
  }

  R invoke_fn(data_t* fn, Args... args) const {
    return cast(fn)(std::forward<Args>(args)...);
  }

  void copy(data_t* src, data_t* dst) const {
    new (dst) F(cast(src));
  }

  void destroy_fn(details::data_t* fn) const noexcept {
    cast_const(fn).~F();
  }

  void move_fn(data_t* src, data_t* dst) const noexcept {
    new (dst) F(std::move(cast(src)));
    destroy_fn(src);
  }
};

template <typename F, typename R, typename... Args>
  requires(!is_small<F>)
struct descriptor<F, R, Args...> : base_descriptor<R, Args...> {
  static F* cast(data_t* obj) noexcept {
    return *std::launder(reinterpret_cast<F**>(obj));
  }

  static const F* cast_const(data_t* obj) noexcept {
    return *std::launder(reinterpret_cast<const F**>(obj));
  }

  static void fill_data(data_t& dst, F&& func) {
    reinterpret_cast<void*&>(dst) = new F(std::move(func));
  }

  static F* get_pointer(data_t& data) {
    return cast(&data);
  }

  R invoke_fn(data_t* fn, Args... args) const {
    return (*cast(fn))(std::forward<Args>(args)...);
  }

  void copy(data_t* src, data_t* dst) const {
    new (dst) F*(new F(*cast_const(src)));
  }

  void destroy_fn(data_t* fn) const noexcept {
    delete cast(fn);
  }

  void move_fn(data_t* src, data_t* dst) const noexcept {
    new (dst) F*(cast(src));
  }
};

template <typename R, typename... Args>
struct descriptor<empty, R, Args...> : base_descriptor<R, Args...> {
  virtual R invoke_fn([[maybe_unused]] data_t* fn, [[maybe_unused]] Args... args) const {
    throw bad_function_call();
  }
};

template <typename F, typename R, typename... Args>
static const base_descriptor<R, Args...>* get_descriptor() {
  static constexpr descriptor<F, R, Args...> temp;
  return &temp;
}
} // namespace details
template <typename F>
class function;

template <typename R, typename... Args>
class function<R(Args...)> {
public:
  function() noexcept : calls(details::get_descriptor<details::empty, R, Args...>()) {}

  template <typename F>
  function(F func) : calls(details::get_descriptor<F, R, Args...>()) {
    details::descriptor<F, R, Args...>::fill_data(data, std::move(func));
  }

  function(const function& other) : calls(other.calls) {
    other.calls->copy(&other.data, &data);
  }

  function(function&& other) noexcept : calls(std::move(other.calls)) {
    other.calls->move_fn(&other.data, &data);
    other.calls = details::get_descriptor<details::empty, R, Args...>();
  }

  function& operator=(const function& other) {
    if (this != &other) {
      this->operator=(function(other));
    }
    return *this;
  }

  function& operator=(function&& other) noexcept {
    if (this != &other) {
      calls->destroy_fn(&data);
      calls = other.calls;
      other.calls->move_fn(&other.data, &data);
      other.calls = details::get_descriptor<details::empty, R, Args...>();
    }
    return *this;
  }

  ~function() {
    calls->destroy_fn(&data);
  }

  explicit operator bool() const noexcept {
    return calls != details::get_descriptor<details::empty, R, Args...>();
  }

  R operator()(Args... args) const {
    return calls->invoke_fn(&data, std::forward<Args>(args)...);
  }

  template <typename T>
  T* target() noexcept {
    if (details::get_descriptor<T, R, Args...>() != calls) {
      return nullptr;
    }
    return details::descriptor<T, R, Args...>::get_pointer(data);
  }

  template <typename T>
  const T* target() const noexcept {
    if (details::get_descriptor<T, R, Args...>() != calls) {
      return nullptr;
    }

    return details::descriptor<T, R, Args...>::get_pointer(data);
  }

private:
  const details::base_descriptor<R, Args...>* calls;
  mutable details::data_t data;
};
