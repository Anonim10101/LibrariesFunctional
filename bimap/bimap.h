#pragma once

#include "intrusive-tree.h"

#include <cstddef>
#include <functional>

template <typename Left, typename Right, typename CompareLeft = std::less<Left>,
          typename CompareRight = std::less<Right>>
class bimap {
public:
  using left_t = Left;
  using right_t = Right;

private:
  struct left;
  struct right;
  template <typename>
  class universal_iterator;

  template <typename ret_type, typename inp_type>
  class getter {
  public:
    static const ret_type& get(const inp_type& inp) {
      return inp.dec;
    }
  };

  template <typename Tag>
  struct template_node : intrusive::tree_element<Tag> {
    using data_type = std::conditional_t<std::is_same_v<Tag, left>, left_t, right_t>;

    template_node(const data_type& inp) : dec(inp) {}

    template_node(data_type&& inp) : dec(std::move(inp)) {}

    using data_tag = Tag;
    using friend_tag = std::conditional_t<std::is_same_v<Tag, left>, right, left>;
    using iterator = universal_iterator<template_node>;
    using friend_name = template_node<friend_tag>;
    using tree_type = intrusive::tree<data_type, template_node, getter<data_type, template_node>, Tag,
                                      std::conditional_t<std::is_same_v<Tag, left>, CompareLeft, CompareRight>>;
    data_type dec;
  };

  using left_node = template_node<left>;
  using right_node = template_node<right>;

  struct linking_node : public intrusive::tree_element<left>, intrusive::tree_element<right> {};

  struct data_node : left_node, right_node {
    explicit data_node(const Left& dec1, const Right& dec2) : left_node(dec1), right_node(dec2) {}

    explicit data_node(const Left& dec1, Right&& dec2) : left_node(dec1), right_node(std::move(dec2)) {}

    explicit data_node(Left&& dec1, const Right& dec2) : left_node(std::move(dec1)), right_node(dec2) {}

    explicit data_node(Left&& dec1, Right&& dec2) : left_node(std::move(dec1)), right_node(std::move(dec2)) {}

    ~data_node() = default;
  };

  template <typename Iter_Type>
  class universal_iterator {
  private:
    template <typename, typename, typename, typename>
    friend class bimap;
    template <typename>
    friend class universal_iterator;

  public:
    using value_type = typename Iter_Type::data_type;
    using reference = const typename Iter_Type::data_type&;
    using pointer = const typename Iter_Type::data_type*;
    using difference_type = std::ptrdiff_t;
    using iterator_category = std::bidirectional_iterator_tag;

    universal_iterator() = default;

    const value_type& operator*() const {
      return (*cur_node).dec;
    }

    const value_type* operator->() const {
      return &(*cur_node).dec;
    }

    universal_iterator& operator++() {
      cur_node++;
      return *this;
    }

    universal_iterator operator++(int) {
      universal_iterator temp = *this;
      ++(*this);
      return temp;
    }

    universal_iterator& operator--() {
      cur_node--;
      return *this;
    }

    universal_iterator operator--(int) {
      universal_iterator temp = *this;
      --(*this);
      return temp;
    }

    typename Iter_Type::friend_name::iterator flip() const {
      if (cur_node.is_end()) {
        return typename Iter_Type::friend_name::iterator(
            static_cast<intrusive::tree_element<typename Iter_Type::friend_tag>*>(static_cast<linking_node*>(
                const_cast<intrusive::tree_element<typename Iter_Type::data_tag>*>(cur_node.get_by_element()))));
      }
      return typename Iter_Type::friend_name::iterator(
          static_cast<typename Iter_Type::friend_name*>(static_cast<data_node*>(cur_node.operator->())));
    }

    friend bool operator==(const universal_iterator& lhs, const universal_iterator& rhs) {
      return lhs.cur_node == rhs.cur_node;
    }

    friend bool operator!=(const universal_iterator& lhs, const universal_iterator& rhs) {
      return !(lhs == rhs);
    }

  private:
    using tree_iterator = typename Iter_Type::tree_type::iterator;

    universal_iterator(intrusive::tree_element<typename Iter_Type::data_tag>* inp) : cur_node(tree_iterator(inp)) {}

    universal_iterator(tree_iterator inp) : cur_node(inp) {}

    tree_iterator cur_node;
  };

public:
  using node_t = data_node;

  using right_iterator = universal_iterator<right_node>;

  using left_iterator = universal_iterator<left_node>;

  // Создает bimap, не содержащий ни одной пары.
  bimap(CompareLeft compare_left = CompareLeft(), CompareRight compare_right = CompareRight())
      : sentinel(),
        left_tree(static_cast<intrusive::tree_element<left>*>(&sentinel), std::move(compare_left)),
        right_tree(static_cast<intrusive::tree_element<right>*>(&sentinel), std::move(compare_right)) {}

  // Конструкторы от других и присваивания
  bimap(const bimap& other)
      : left_tree(&sentinel, CompareLeft(other.left_tree.get_comp())),
        right_tree(&sentinel, CompareRight(other.right_tree.get_comp())) {
    try {
      for (auto cur_node = other.begin_left(); cur_node != other.end_left(); cur_node++) {
        insert(*cur_node, *cur_node.flip());
      }
    } catch (...) {
      erase_left(begin_left(), end_left());
      throw;
    }
  }

  bimap(bimap&& other) noexcept
      : elements_num(std::move(other.elements_num)),
        sentinel(),
        left_tree(&sentinel, std::move(other.left_tree)),
        right_tree(&sentinel, std::move(other.right_tree)) {
    other.elements_num = 0;
  }

  bimap& operator=(const bimap& other) {
    if (other != *this) {
      auto temp(other);
      swap(*this, temp);
    }
    return *this;
  }

  bimap& operator=(bimap&& other) noexcept {
    swap(other, *this);
    return *this;
  }

  // Деструктор. Вызывается при удалении объектов bimap.
  // Инвалидирует все итераторы ссылающиеся на элементы этого bimap
  // (включая итераторы ссылающиеся на элементы следующие за последними).
  ~bimap() {
    erase_left(begin_left(), end_left());
  }

  friend void swap(bimap& lhs, bimap& rhs) {
    std::swap(lhs.elements_num, rhs.elements_num);
    lhs.left_tree.swap(rhs.left_tree);
    lhs.right_tree.swap(rhs.right_tree);
  }

  // Вставка пары (left, right), возвращает итератор на left.
  // Если такой left или такой right уже присутствуют в bimap, вставка не
  // производится и возвращается end_left().
  template <typename L = Left, typename R = Right>
  left_iterator insert(L&& left, R&& right) {
    auto res = left_tree.begin();
    if (find_right(right) == end_right() && find_left(left) == end_left()) {
      auto* to_ins = new data_node(std::forward<L>(left), std::forward<R>(right));
      try {
        res = left_tree.insert(static_cast<left_node*>(to_ins));
        try {
          right_tree.insert(static_cast<right_node*>(to_ins));
        } catch (...) {
          left_tree.erase(res);
          throw;
        }
      } catch (...) {
        delete to_ins;
        throw;
      }
      elements_num++;
      return left_iterator(res);
    }
    return end_left();
  }

  // Удаляет элемент и соответствующий ему парный.
  // erase невалидного итератора не определен.
  // erase(end_left()) и erase(end_right()) не определены.
  // Пусть it ссылается на некоторый элемент e.
  // erase инвалидирует все итераторы ссылающиеся на e и на элемент парный к e.
  left_iterator erase_left(left_iterator it) {
    right_tree.erase(it.flip().cur_node);
    auto old = it++;
    delete static_cast<data_node*>(left_tree.erase(old.cur_node));
    elements_num--;
    return left_iterator(it.cur_node);
  }

  // Аналогично erase, но по ключу, удаляет элемент если он присутствует, иначе
  // не делает ничего Возвращает была ли пара удалена
  bool erase_left(const left_t& left) {
    auto temp = find_left(left);
    if (temp == end_left()) {
      return false;
    }
    erase_left(temp);
    return true;
  }

  right_iterator erase_right(right_iterator it) {
    left_tree.erase(it.flip().cur_node);
    auto old = it++;
    delete static_cast<data_node*>(right_tree.erase(old.cur_node));
    elements_num--;
    return right_iterator(it.cur_node);
  }

  bool erase_right(const right_t& right) {
    auto temp = find_right(right);
    if (temp == end_right()) {
      return false;
    }
    erase_right(temp);
    return true;
  }

  // erase от ренжа, удаляет [first, last), возвращает итератор на последний
  // элемент за удаленной последовательностью
  left_iterator erase_left(left_iterator first, left_iterator last) {
    for (auto iter = first; iter != last; iter = erase_left(iter)) {}
    return last;
  }

  right_iterator erase_right(right_iterator first, right_iterator last) {
    for (auto iter = first; iter != last; iter = erase_right(iter)) {}
    return last;
  }

  // Возвращает итератор по элементу. Если не найден - соответствующий end()
  left_iterator find_left(const left_t& left) const {
    return left_tree.find(left);
  }

  right_iterator find_right(const right_t& right) const {
    return right_tree.find(right);
  }

  // Возвращает противоположный элемент по элементу
  // Если элемента не существует -- бросает std::out_of_range
  const right_t& at_left(const left_t& key) const {
    auto res = find_left(key).flip();
    if (res == end_right()) {
      throw std::out_of_range("key not found");
    }
    return *res;
  }

  const left_t& at_right(const right_t& key) const {
    auto res = find_right(key).flip();
    if (res == end_left()) {
      throw std::out_of_range("key not found");
    }
    return *res;
  }

  // Возвращает противоположный элемент по элементу
  // Если элемента не существует, добавляет его в bimap и на противоположную
  // сторону кладет дефолтный элемент, ссылку на который и возвращает
  // Если дефолтный элемент уже лежит в противоположной паре - должен поменять
  // соответствующий ему элемент на запрашиваемый (смотри тесты)
  template <typename checker = right_t>
    requires(std::is_default_constructible_v<checker>)
  const right_t& at_left_or_default(const left_t& key) {
    auto res = find_left(key).flip();
    if (res == end_right()) {
      res = find_right(Right());
      if (res == end_right()) {
        res = insert(key, Right()).flip();
      } else {
        auto to_change = static_cast<data_node*>(right_tree.get_node(res.cur_node));
        left_tree.erase(res.flip().cur_node);
        static_cast<left_node*>(to_change)->dec.~Left();
        new (&static_cast<left_node*>(to_change)->dec) Left(key);
        left_tree.insert(to_change);
      }
    }
    return *res;
  }

  template <typename checker = left_t>
    requires(std::is_default_constructible_v<checker>)
  const left_t& at_right_or_default(const right_t& key) {
    auto res = find_right(key).flip();
    if (res == end_left()) {
      res = find_left(Left());
      if (res == end_left()) {
        res = insert(Left(), key);
      } else {
        auto to_change = static_cast<data_node*>(left_tree.get_node(res.cur_node));
        right_tree.erase(res.flip().cur_node);
        static_cast<right_node*>(to_change)->dec.~Right();
        new (&static_cast<right_node*>(to_change)->dec) Right(key);
        right_tree.insert(to_change);
      }
    }
    return *res;
  }

  // lower и upper bound'ы по каждой стороне
  // Возвращают итераторы на соответствующие элементы
  // Смотри std::lower_bound, std::upper_bound.
  left_iterator lower_bound_left(const left_t& left) const {
    return left_iterator(left_tree.lower_bound(left));
  }

  left_iterator upper_bound_left(const left_t& left) const {
    return left_iterator(left_tree.upper_bound(left));
  }

  right_iterator lower_bound_right(const right_t& left) const {
    return right_iterator(right_tree.lower_bound(left));
  }

  right_iterator upper_bound_right(const right_t& left) const {
    return right_iterator(right_tree.upper_bound(left));
  }

  // Возващает итератор на минимальный по порядку left.
  left_iterator begin_left() const {
    return left_iterator(left_tree.begin());
  }

  // Возващает итератор на следующий за последним по порядку left.
  left_iterator end_left() const {
    return left_iterator(left_tree.end());
  }

  // Возващает итератор на минимальный по порядку right.
  right_iterator begin_right() const {
    return right_iterator(right_tree.begin());
  }

  // Возващает итератор на следующий за последним по порядку right.
  right_iterator end_right() const {
    return right_iterator(right_tree.end());
  }

  // Проверка на пустоту
  bool empty() const {
    return left_tree.empty();
  }

  // Возвращает размер бимапы (кол-во пар)
  std::size_t size() const {
    return elements_num;
  }

  // Операторы сравнения
  friend bool operator==(const bimap& lhs, const bimap& rhs) {
    if (lhs.size() != rhs.size()) {
      return false;
    }
    for (auto iter_l = lhs.begin_left(), iter_r = rhs.begin_left(); iter_l != lhs.end_left(); iter_l++, iter_r++) {
      if (lhs.left_tree.get_comp()(*iter_l, *iter_r) || lhs.left_tree.get_comp()(*iter_r, *iter_l) ||
          lhs.right_tree.get_comp()(*iter_l.flip(), *iter_r.flip()) ||
          lhs.right_tree.get_comp()(*iter_r.flip(), *iter_l.flip())) {
        return false;
      }
    }
    return true;
  }

  friend bool operator!=(const bimap& lhs, const bimap& rhs) {
    return !(lhs == rhs);
  }

private:
  size_t elements_num{0};
  linking_node sentinel;
  intrusive::tree<left_t, left_node, getter<left_t, left_node>, left, CompareLeft> left_tree{
      static_cast<intrusive::tree_element<left>*>(&sentinel)};
  intrusive::tree<right_t, right_node, getter<right_t, right_node>, right, CompareRight> right_tree{
      static_cast<intrusive::tree_element<right>*>(&sentinel)};
};