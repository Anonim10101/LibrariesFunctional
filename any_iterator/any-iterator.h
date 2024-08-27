#pragma once

#include <cstddef>
#include <iterator>
#include <type_traits>

class operation_with_empty_it : public std::exception {
  const char* what() const noexcept override {
    return "Called some operation with empty operator ";
  }
};

namespace details {

using data_t = std::aligned_storage_t<sizeof(std::max_align_t), alignof(std::max_align_t)>;

struct empty {
  empty operator--() {
    throw operation_with_empty_it();
  }
};

std::ptrdiff_t operator-(empty, empty) {
  throw operation_with_empty_it();
}

bool operator<(empty, empty) {
  throw operation_with_empty_it();
}

void operator+=(empty, int) {
  throw operation_with_empty_it();
}

template <typename F>
static constexpr bool is_small = (sizeof(F) <= sizeof(data_t)) && std::is_nothrow_move_constructible_v<F>;

template <typename T>
struct base_descriptor {
  virtual void copy([[maybe_unused]] data_t* src, [[maybe_unused]] data_t* dst) const {}

  virtual void destroy_it([[maybe_unused]] data_t* fn) const noexcept {}

  virtual void move_it([[maybe_unused]] data_t* src, [[maybe_unused]] data_t* dst) const noexcept {}

  virtual T& get_element([[maybe_unused]] data_t* it) const = 0;

  virtual void inc([[maybe_unused]] data_t* it) const = 0;

  virtual void dec([[maybe_unused]] data_t* it) const {
    throw operation_with_empty_it();
  }

  virtual bool is_eq([[maybe_unused]] data_t* it, [[maybe_unused]] data_t* other) const {
    throw operation_with_empty_it();
  }

  virtual bool is_less([[maybe_unused]] data_t* it, [[maybe_unused]] data_t* other) const {
    throw operation_with_empty_it();
  }

  virtual void add([[maybe_unused]] data_t* it, [[maybe_unused]] int32_t num) const {
    throw operation_with_empty_it();
  }

  virtual std::ptrdiff_t sub([[maybe_unused]] data_t* it1, [[maybe_unused]] data_t* it2) const {
    throw operation_with_empty_it();
  }
};

template <typename, typename, typename>
struct descriptor {};

template <typename T>
struct descriptor<T, empty, std::forward_iterator_tag> : base_descriptor<T> {
  static empty& cast([[maybe_unused]] data_t* obj) {
    throw operation_with_empty_it();
  }

  T& get_element([[maybe_unused]] data_t* it) const override {
    throw operation_with_empty_it();
  }

  void inc([[maybe_unused]] data_t* it) const override {
    throw operation_with_empty_it();
  }
};

template <typename T, typename It>
struct descriptor<T, It, std::forward_iterator_tag> : base_descriptor<T> {
  static It& cast(data_t* obj) noexcept {
    return *std::launder(reinterpret_cast<It*>(obj));
  }

  static const It& cast_const(data_t* obj) noexcept {
    return *std::launder(reinterpret_cast<const It*>(obj));
  }

  static void fill_data(data_t& dst, It&& it) {
    new (&dst) It(std::move(it));
  }

  void inc(data_t* it) const override {
    cast(it).operator++();
  }

  T& get_element(data_t* it) const override {
    return cast(it).operator*();
  }

  virtual bool is_eq(data_t* it, data_t* other) const override {
    return cast(it) == cast(other);
  }

  void copy(data_t* src, data_t* dst) const override {
    new (dst) It(cast(src));
  }

  void destroy_it(data_t* it) const noexcept override {
    cast_const(it).~It();
  }

  void move_it(data_t* src, data_t* dst) const noexcept override {
    new (dst) It(std::move(cast(src)));
    destroy_it(src);
  }
};

template <typename T, typename It>
  requires(!is_small<It>)
struct descriptor<T, It, std::forward_iterator_tag> : base_descriptor<T> {
  static It* cast(data_t* obj) noexcept {
    return *std::launder(reinterpret_cast<It**>(obj));
  }

  static const It* cast_const(data_t* obj) noexcept {
    return *std::launder(reinterpret_cast<const It**>(obj));
  }

  static void fill_data(data_t& dst, It&& it) {
    reinterpret_cast<void*&>(dst) = new It(std::move(it));
  }

  void inc(data_t* it) const override {
    (*cast(it))++;
  }

  virtual bool is_eq(data_t* it, data_t* other) const override {
    return (*cast(it)) == (*cast(other));
  }

  T& get_element(data_t* it) const override {
    return *(*cast(it));
  }

  void copy(data_t* src, data_t* dst) const override {
    new (dst) It*(new It(*cast_const(src)));
  }

  void destroy_it(data_t* it) const noexcept override {
    delete cast(it);
  }

  void move_it(data_t* src, data_t* dst) const noexcept override {
    new (dst) It*(cast(src));
  }
};

template <typename T, typename It>
struct descriptor<T, It, std::bidirectional_iterator_tag> : descriptor<T, It, std::forward_iterator_tag> {
  virtual void dec([[maybe_unused]] data_t* it) const override {
    if constexpr (is_small<It>) {
      descriptor<T, It, std::forward_iterator_tag>::cast(it).operator--();
    } else {
      descriptor<T, It, std::forward_iterator_tag>::cast(it)->operator--();
    }
  }
};

template <typename T, typename It>
struct descriptor<T, It, std::random_access_iterator_tag> : descriptor<T, It, std::bidirectional_iterator_tag> {
  virtual bool is_less(data_t* it, data_t* other) const override {
    if constexpr (is_small<It>) {
      return descriptor<T, It, std::forward_iterator_tag>::cast(it) <
             descriptor<T, It, std::forward_iterator_tag>::cast(other);
    } else {
      return (*descriptor<T, It, std::forward_iterator_tag>::cast(it)) <
             (*descriptor<T, It, std::forward_iterator_tag>::cast(other));
    }
  }

  virtual void add([[maybe_unused]] data_t* it, int num) const override {
    if constexpr (is_small<It>) {
      descriptor<T, It, std::forward_iterator_tag>::cast(it) += num;
    } else {
      (*descriptor<T, It, std::forward_iterator_tag>::cast(it)) += num;
    }
  }

  virtual std::ptrdiff_t sub([[maybe_unused]] data_t* it1, [[maybe_unused]] data_t* it2) const override {
    if constexpr (is_small<It>) {
      return descriptor<T, It, std::forward_iterator_tag>::cast(it1) -
             descriptor<T, It, std::forward_iterator_tag>::cast(it2);
    } else {
      return (*descriptor<T, It, std::forward_iterator_tag>::cast(it1)) -
             (*descriptor<T, It, std::forward_iterator_tag>::cast(it2));
    }
  }
};

template <typename T, typename It, typename Tag>
static const base_descriptor<T>* get_descriptor() {
  static constexpr descriptor<T, It, Tag> temp;
  return &temp;
}

} // namespace details

template <typename T, typename Tag>
class any_iterator {
public:
  using value_type = std::remove_cvref_t<T>;
  using pointer = T*;
  using reference = T&;
  using difference_type = std::ptrdiff_t;
  using iterator_category = Tag;

public:
  any_iterator() : calls(details::get_descriptor<T, details::empty, Tag>()) {}

  ~any_iterator() {
    calls->destroy_it(&data);
  }

  template <typename It>
  any_iterator(It inp) : calls(details::get_descriptor<T, It, Tag>()) {
    details::descriptor<T, It, Tag>::fill_data(data, std::move(inp));
  }

  any_iterator(const any_iterator& other) : calls(other.calls) {
    other.calls->copy(&other.data, &data);
  }

  any_iterator(any_iterator&& other) : calls(std::move(other.calls)) {
    other.calls->move_it(&other.data, &data);
    other.calls = details::get_descriptor<T, details::empty, Tag>();
  }

  template <typename It>
  any_iterator& operator=(It it) {
    calls->destroy_it(&data);
    calls(details::get_descriptor<T, It, Tag>());
    details::descriptor<T, It, Tag>::fill_data(data, std::move(any_iterator(it)));
  }

  any_iterator& operator=(any_iterator&& other) {
    if (this != &other) {
      calls->destroy_it(&data);
      calls = other.calls;
      other.calls->move_it(&other.data, &data);
      other.calls = details::get_descriptor<T, details::empty, Tag>();
    }
    return *this;
  }

  any_iterator& operator=(const any_iterator& other) {
    this->operator=(any_iterator(other));
    return *this;
  }

  void swap(any_iterator& other) noexcept {
    std::swap(other.calls, calls);
    std::swap(other.data, data);
  }

  reference operator*() const {
    return calls->get_element(&data);
  }

  reference operator*() {
    return calls->get_element(&data);
  }

  pointer operator->() const {
    return &(calls->get_element(&data));
  }

  pointer operator->() {
    return &(calls->get_element(&data));
  }

  any_iterator& operator++() & {
    calls->inc(&data);
    return *this;
  }

  any_iterator operator++(int) & {
    auto res = *this;
    ++(*this);
    return res;
  }

  reference operator[](size_t num)
    requires(std::is_same_v<std::random_access_iterator_tag, Tag>)
  {
    return *(any_iterator(*this) + num);
  }

  reference operator[](size_t num) const
    requires(std::is_same_v<std::random_access_iterator_tag, Tag>)
  {
    return *(any_iterator(*this) + num);
  }

  template <typename TT, typename TTag>
  friend bool operator==(const any_iterator<TT, TTag>& a, const any_iterator<TT, TTag>& b);

  template <typename TT, typename TTag>
  friend bool operator!=(const any_iterator<TT, TTag>& a, const any_iterator<TT, TTag>& b);

  // note: following operators must compile ONLY for appropriate iterator tags

  template <typename TT, typename TTag>
    requires(std::is_same_v<std::bidirectional_iterator_tag, TTag> ||
             std::is_same_v<std::random_access_iterator_tag, TTag>)
  friend any_iterator<TT, TTag>& operator--(any_iterator<TT, TTag>&);

  template <typename TT, typename TTag>
    requires(std::is_same_v<std::bidirectional_iterator_tag, TTag> ||
             std::is_same_v<std::random_access_iterator_tag, TTag>)
  friend any_iterator<TT, TTag> operator--(any_iterator<TT, TTag>&, int);

  template <typename TT, typename TTag>
    requires(std::is_same_v<std::random_access_iterator_tag, TTag>)
  friend any_iterator<TT, TTag>& operator+=(any_iterator<TT, TTag>&, typename any_iterator<TT, TTag>::difference_type);

  template <typename TT, typename TTag>
    requires(std::is_same_v<std::random_access_iterator_tag, TTag>)
  friend any_iterator<TT, TTag>& operator-=(any_iterator<TT, TTag>&, typename any_iterator<TT, TTag>::difference_type);

  template <typename TT, typename TTag>
    requires(std::is_same_v<std::random_access_iterator_tag, TTag>)
  friend any_iterator<TT, TTag> operator+(any_iterator<TT, TTag>, typename any_iterator<TT, TTag>::difference_type);

  template <typename TT, typename TTag>
    requires(std::is_same_v<std::random_access_iterator_tag, TTag>)
  friend any_iterator<TT, TTag> operator-(any_iterator<TT, TTag> it,
                                          typename any_iterator<TT, TTag>::difference_type dif);

  template <typename TT, typename TTag>
    requires(std::is_same_v<std::random_access_iterator_tag, TTag>)
  friend bool operator<(const any_iterator<TT, TTag>& a, const any_iterator<TT, TTag>& b);

  template <typename TT, typename TTag>
    requires(std::is_same_v<std::random_access_iterator_tag, TTag>)
  friend bool operator>(const any_iterator<TT, TTag>& a, const any_iterator<TT, TTag>& b);

  template <typename TT, typename TTag>
    requires(std::is_same_v<std::random_access_iterator_tag, TTag>)
  friend bool operator<=(const any_iterator<TT, TTag>& a, const any_iterator<TT, TTag>& b);

  template <typename TT, typename TTag>
    requires(std::is_same_v<std::random_access_iterator_tag, TTag>)
  friend bool operator>=(const any_iterator<TT, TTag>& a, const any_iterator<TT, TTag>& b);

  template <typename TT, typename TTag>
    requires(std::is_same_v<std::random_access_iterator_tag, TTag>)
  friend typename any_iterator<TT, TTag>::difference_type operator-(const any_iterator<TT, TTag>& a,
                                                                    const any_iterator<TT, TTag>& b);

private:
  const details::base_descriptor<T>* calls;
  mutable details::data_t data;
};

template <typename TT, typename TTag>
  requires(std::is_same_v<std::random_access_iterator_tag, TTag>)
any_iterator<TT, TTag>::difference_type operator-(const any_iterator<TT, TTag>& a, const any_iterator<TT, TTag>& b) {
  return a.calls->sub(&a.data, &b.data);
}

template <typename TT, typename TTag>
  requires(std::is_same_v<std::random_access_iterator_tag, TTag>)
bool operator<(const any_iterator<TT, TTag>& a, const any_iterator<TT, TTag>& b) {
  return (a.calls->is_less(&a.data, &b.data));
}

template <typename TT, typename TTag>
  requires(std::is_same_v<std::random_access_iterator_tag, TTag>)
bool operator>(const any_iterator<TT, TTag>& a, const any_iterator<TT, TTag>& b) {
  return b < a;
}

template <typename TT, typename TTag>
  requires(std::is_same_v<std::random_access_iterator_tag, TTag>)
bool operator<=(const any_iterator<TT, TTag>& a, const any_iterator<TT, TTag>& b) {
  return !(a > b);
}

template <typename TT, typename TTag>
  requires(std::is_same_v<std::random_access_iterator_tag, TTag>)
bool operator>=(const any_iterator<TT, TTag>& a, const any_iterator<TT, TTag>& b) {
  return !(a < b);
}

template <typename TT, typename TTag>
  requires(std::is_same_v<std::random_access_iterator_tag, TTag>)
any_iterator<TT, TTag> operator-(any_iterator<TT, TTag> inp, typename any_iterator<TT, TTag>::difference_type to_sub) {
  any_iterator<TT, TTag> temp(inp);
  temp -= to_sub;
  return temp;
}

template <typename TT, typename TTag>
  requires(std::is_same_v<std::random_access_iterator_tag, TTag>)
any_iterator<TT, TTag>& operator+=(any_iterator<TT, TTag>& inp,
                                   typename any_iterator<TT, TTag>::difference_type to_add) {
  inp.calls->add(&inp.data, to_add);
  return inp;
}

template <typename TT, typename TTag>
  requires(std::is_same_v<std::random_access_iterator_tag, TTag>)
any_iterator<TT, TTag> operator+(any_iterator<TT, TTag> inp, typename any_iterator<TT, TTag>::difference_type to_add) {
  any_iterator<TT, TTag> temp(inp);
  temp += to_add;
  return temp;
}

template <typename TT, typename TTag>
  requires(std::is_same_v<std::random_access_iterator_tag, TTag>)
any_iterator<TT, TTag> operator+(typename any_iterator<TT, TTag>::difference_type to_add, any_iterator<TT, TTag> inp) {
  return inp + to_add;
}

template <typename TT, typename TTag>
  requires(std::is_same_v<std::random_access_iterator_tag, TTag>)
any_iterator<TT, TTag>& operator-=(any_iterator<TT, TTag>& inp,
                                   typename any_iterator<TT, TTag>::difference_type to_sub) {
  inp += -to_sub;
  return inp;
}

template <typename TT, typename TTag>
  requires(std::is_same_v<std::bidirectional_iterator_tag, TTag> ||
           std::is_same_v<std::random_access_iterator_tag, TTag>)
any_iterator<TT, TTag> operator--(any_iterator<TT, TTag>& inp, int) {
  auto res = inp;
  --inp;
  return res;
}

template <typename TT, typename TTag>
  requires(std::is_same_v<std::bidirectional_iterator_tag, TTag> ||
           std::is_same_v<std::random_access_iterator_tag, TTag>)
any_iterator<TT, TTag>& operator--(any_iterator<TT, TTag>& inp) {
  inp.calls->dec(&inp.data);
  return inp;
}

template <typename TT, typename TTag>
bool operator==(const any_iterator<TT, TTag>& a, const any_iterator<TT, TTag>& b) {
  return a.calls->is_eq(&a.data, &b.data);
}

template <typename TT, typename TTag>
bool operator!=(const any_iterator<TT, TTag>& a, const any_iterator<TT, TTag>& b) {
  return !(a == b);
}