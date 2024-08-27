#pragma once

#include <cstddef>
#include <iterator>
#include <type_traits>

namespace intrusive {

class default_tag;

struct base_node {
public:
  base_node();
  ~base_node();
  base_node(base_node&& other);
  base_node& operator=(base_node&& other);
  base_node(const base_node&);
  base_node& operator=(const base_node& other);
  void del_link();
  void link_back(base_node* other);
  base_node* get_next() const;
  base_node* get_prev() const;

private:
  base_node* prev = this;
  base_node* next = this;
  void unlink() noexcept;
};

template <typename Tag = default_tag>
class list_element : private base_node {
  template <typename T, typename U>
  friend class list;
};

template <typename T, typename Tag = default_tag>
class list {
  static_assert(std::is_base_of_v<list_element<Tag>, T>, "T must derive from list_element");

private:
  template <class E>
  struct base_iterator {
    using value_type = T;
    using reference = E&;
    using pointer = E*;
    using difference_type = std::ptrdiff_t;
    using iterator_category = std::bidirectional_iterator_tag;
    friend list;
    base_iterator() = default;
    base_iterator(const base_iterator& other) = default;
    base_iterator(base_iterator&& other) = default;

    base_iterator& operator=(base_iterator&& other) = default;
    base_iterator& operator=(const base_iterator& other) = default;

    base_iterator& operator++() {
      _elem = _elem->get_next();
      return *this;
    }

    base_iterator operator++(int) {
      base_iterator temp = *this;
      ++*this;
      return temp;
    }

    base_iterator& operator--() {
      _elem = _elem->get_prev();
      return *this;
    }

    base_iterator operator--(int) {
      base_iterator temp = *this;
      --*this;
      return temp;
    }

    pointer operator->() const {
      return static_cast<E*>(static_cast<list_element<Tag>*>(_elem));
    }

    friend bool operator==(const base_iterator& a, const base_iterator& b) {
      return (a._elem == b._elem);
    }

    friend bool operator!=(const base_iterator& a, const base_iterator& b) {
      return a._elem != b._elem;
    }

    reference operator*() const {
      return *static_cast<E*>(static_cast<list_element<Tag>*>(_elem));
    }

    operator base_iterator<const E>() const {
      return base_iterator<const E>(_elem);
    }

    ~base_iterator() = default;

  private:
    base_iterator(base_node* element) : _elem(element) {}

    base_node* _elem;
  };

public:
  using iterator = base_iterator<T>;
  using const_iterator = base_iterator<const T>;

  list() noexcept {}

  ~list() {
    clear();
  }

  list(const list&) = delete;
  list& operator=(const list&) = delete;

  list(list&& other) noexcept = default;

  list& operator=(list&& other) noexcept = default;

  bool empty() const noexcept {
    return sentinel.get_prev() == static_cast<const base_node*>(&sentinel);
  }

  size_t size() const noexcept {
    return std::distance(begin(), end());
  }

  T& front() noexcept {
    return *begin();
  }

  const T& front() const noexcept {
    return *begin();
  }

  T& back() noexcept {
    return *(--end());
  }

  const T& back() const noexcept {
    return *(--end());
  }

  void push_front(T& value) noexcept {
    insert(begin(), value);
  }

  void push_back(T& value) noexcept {
    insert(end(), value);
  }

  void pop_front() noexcept {
    erase(begin());
  }

  void pop_back() noexcept {
    erase(--end());
  }

  void clear() noexcept {
    sentinel.del_link();
  }

  iterator begin() noexcept {
    return base_iterator<T>(static_cast<base_node*>(&sentinel)->get_next());
  }

  const_iterator begin() const noexcept {
    return base_iterator<const T>(const_cast<base_node*>(static_cast<const base_node*>(&sentinel))->get_next());
  }

  iterator end() noexcept {
    return base_iterator<T>(static_cast<base_node*>(&sentinel));
  }

  const_iterator end() const noexcept {
    return base_iterator<const T>(const_cast<base_node*>(static_cast<const base_node*>(&sentinel)));
  }

  iterator insert(const_iterator pos, T& value) noexcept {
    base_node* base = static_cast<list_element<Tag>*>(&value);
    base_node* before_node = static_cast<list_element<Tag>*>(pos._elem);
    if (base == before_node) {
      return base_iterator<T>(before_node);
    }
    base->del_link();
    base->link_back(before_node->get_prev());
    before_node->link_back(base);
    return base_iterator<T>(before_node->get_prev());
  }

  iterator erase(const_iterator pos) noexcept {
    iterator res = base_iterator<T>(pos._elem->get_next());
    pos._elem->del_link();
    return res;
  }

  void splice(const_iterator pos, list&, const_iterator first, const_iterator last) noexcept {
    if (last != first) {
      base_node* lastIns = last._elem->get_prev();
      last._elem->link_back(first._elem->get_prev());
      base_node* firstIn = pos._elem->get_prev();
      pos._elem->link_back(lastIns);
      first._elem->link_back(firstIn);
    }
  }

  iterator get_iterator(T& inp) {
    return iterator(static_cast<list_element<Tag>*>(&inp));
  }

private:
  list_element<Tag> sentinel{};
};

} // namespace intrusive
