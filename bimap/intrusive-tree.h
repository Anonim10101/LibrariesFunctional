#pragma once

#include <cstddef>
#include <functional>
#include <iterator>
#include <type_traits>

namespace intrusive {

class default_tag;

struct base_node {
  template <typename, typename, typename, typename, typename>
  friend class tree;

public:
  base_node() : left_son(this), right_son(this), parent(this) {}

  ~base_node() {
    del_link();
  }

  base_node(base_node&& other) : left_son(other.left_son), right_son(other.right_son), parent(other.parent) {}

  base_node& operator=(base_node&& other) = default;

  base_node(const base_node&) = delete;
  base_node& operator=(const base_node& other) = delete;

  void del_link() {
    if (left_son == this && right_son == this) {
      rem_from_parent();
    } else if (right_son == this) {
      replace(this, left_son);
    } else if (left_son == this) {
      replace(this, right_son);
    } else {
      base_node* temp = get_min_right();
      if (temp->right_son == temp) {
        temp->rem_from_parent();
      } else {
        replace(temp, temp->right_son);
      }
      link_l(left_son, temp);
      if (right_son != this) {
        link_r(right_son, temp);
      }
      replace(this, temp);
    }
    unlink();
  }

  base_node* get_next() {
    base_node* cur_node = this;
    if (cur_node->parent == cur_node) {
      return get_most_left(cur_node->left_son);
    }
    if (cur_node->right_son != this) {
      cur_node = get_most_left(cur_node->right_son);
    } else {
      if (cur_node->parent->left_son == cur_node) {
        cur_node = cur_node->parent;
      } else {
        while (cur_node->parent->right_son == cur_node) {
          cur_node = cur_node->parent;
        }
        cur_node = cur_node->parent;
      }
    }
    return cur_node;
  }

  base_node* get_prev() {
    base_node* cur_node = this;
    if (cur_node->left_son != this) {
      cur_node = cur_node->left_son;
      while (cur_node->right_son != cur_node) {
        cur_node = cur_node->right_son;
      }
    } else if (cur_node->parent->right_son == cur_node) {
      cur_node = cur_node->parent;
    } else {
      while (cur_node->parent->left_son == cur_node) {
        cur_node = cur_node->parent;
      }
      cur_node = cur_node->parent;
    }
    return cur_node;
  }

  static base_node* get_most_left(base_node* node) {
    base_node* cur_node = node;
    while (cur_node->left_son != cur_node) {
      cur_node = cur_node->left_son;
    }
    return cur_node;
  }

  base_node* get_min_right() {
    base_node* cur_node = this;
    if (cur_node->right_son != this) {
      cur_node = cur_node->right_son;
    }
    cur_node = get_most_left(cur_node);
    return cur_node;
  }

private:
  base_node* left_son = this;
  base_node* right_son = this;
  base_node* parent = this;

  static void replace(base_node* old_son, base_node* new_son) {
    if (old_son->parent->left_son == old_son) {
      link_l(new_son, old_son->parent);
    } else {
      link_r(new_son, old_son->parent);
    }
  }

  void rem_from_parent() {
    if (parent->left_son == this) {
      parent->left_son = parent;
    } else {
      parent->right_son = parent;
    }
  }

  void unlink() noexcept {
    parent = this;
    left_son = this;
    right_son = this;
  }

  static void link_r(base_node* rson, base_node* parent) {
    parent->right_son = rson;
    rson->parent = parent;
  }

  static void link_l(base_node* lson, base_node* parent) {
    parent->left_son = lson;
    lson->parent = parent;
  }
};

template <typename Tag = default_tag>
class tree_element : private base_node {
  template <typename, typename, typename, typename, typename>
  friend class tree;
};

template <typename Data_type, typename Node_type, typename Get, typename Tag = default_tag,
          typename Compare = std::less<Node_type>>
class tree {
  static_assert(std::is_base_of_v<tree_element<Tag>, Node_type>, "Node_type must derive from tree_element");

private:
  enum class compare_res {
    greater,
    less,
    equal
  };

  compare_res compare_data(const Data_type& lhs, const Data_type& rhs) const {
    if (comparator(lhs, rhs)) {
      return compare_res::less;
    } else if (comparator(rhs, lhs)) {
      return compare_res::greater;
    } else {
      return compare_res::equal;
    }
  }

  template <class E>
  struct tree_iterator {
    using value_type = Node_type;
    using reference = E&;
    using pointer = E*;
    using difference_type = std::ptrdiff_t;
    using iterator_category = std::bidirectional_iterator_tag;
    friend tree;
    tree_iterator() = default;
    tree_iterator(const tree_iterator& other) = default;
    tree_iterator(tree_iterator&& other) = default;

    tree_iterator(tree_element<Tag>* node) : _elem(static_cast<base_node*>(node)) {}

    tree_iterator& operator=(tree_iterator&& other) = default;
    tree_iterator& operator=(const tree_iterator& other) = default;

    tree_iterator& operator++() {
      _elem = _elem->get_next();
      return *this;
    }

    tree_iterator operator++(int) {
      tree_iterator temp = *this;
      ++*this;
      return temp;
    }

    tree_iterator& operator--() {
      _elem = _elem->get_prev();
      return *this;
    }

    tree_iterator operator--(int) {
      tree_iterator temp = *this;
      --*this;
      return temp;
    }

    pointer operator->() const {
      return static_cast<E*>(static_cast<tree_element<Tag>*>(_elem));
    }

    friend bool operator==(const tree_iterator& a, const tree_iterator& b) {
      return (a._elem == b._elem);
    }

    friend bool operator!=(const tree_iterator& a, const tree_iterator& b) {
      return !(a == b);
    }

    reference operator*() const {
      return *static_cast<E*>(static_cast<tree_element<Tag>*>(_elem));
    }

    operator tree_iterator<const E>() const {
      return tree_iterator<const E>(_elem);
    }

    bool is_end() const {
      return _elem->parent == _elem;
    }

    tree_element<Tag>* get_by_element() const {
      return static_cast<tree_element<Tag>*>(_elem);
    }

    ~tree_iterator() = default;

  private:
    tree_iterator(base_node* element) : _elem(element) {}

    base_node* _elem;
  };

public:
  using iterator = tree_iterator<Node_type>;
  using const_iterator = tree_iterator<const Node_type>;

  tree(tree_element<Tag>* head, Compare&& input_comp) noexcept : comparator(std::move(input_comp)), sentinel(head) {}

  ~tree() {
    clear();
  }

  tree(const tree&) = delete;
  tree& operator=(const tree&) = delete;

  tree(tree_element<Tag>* senti, tree&& other) noexcept : comparator(std::move(other.comparator)), sentinel(senti) {
    if (!other.empty()) {
      base_node::link_l(other.sentinel->left_son, sentinel);
    }
    other.sentinel->left_son = other.sentinel;
  }

  tree& operator=(tree&& other) noexcept = default;

  bool empty() const noexcept {
    return sentinel->left_son == sentinel;
  }

  size_t size() const noexcept {
    return std::distance(begin(), end());
  }

  void clear() noexcept {
    static_cast<tree_element<Tag>*>(sentinel)->del_link();
  }

  iterator begin() const noexcept {
    return iterator(static_cast<base_node*>(static_cast<tree_element<Tag>*>(sentinel))->get_next());
  }

  iterator end() const noexcept {
    return iterator(static_cast<base_node*>(sentinel));
  }

  void swap(tree& other) {
    base_node* temp = sentinel->left_son;
    if (!other.empty()) {
      base_node::link_l(other.sentinel->left_son, sentinel);
    } else {
      sentinel->left_son = sentinel;
    }
    if (temp != sentinel) {
      base_node::link_l(temp, other.sentinel);
    } else {
      other.sentinel->left_son = other.sentinel;
    }
  }

  iterator insert(Node_type* value) noexcept {
    if (empty()) {
      base_node::link_l(static_cast<tree_element<Tag>*>(value), sentinel);
      return iterator(static_cast<tree_element<Tag>*>(value));
    }
    auto pos_to_ins = find_to_insert(Get::get(*value));
    if (pos_to_ins.second == compare_res::equal) {
      return end();
    } else if (pos_to_ins.second == compare_res::less) {
      base_node::link_l(static_cast<tree_element<Tag>*>(value), pos_to_ins.first._elem);
    } else {
      base_node::link_r(static_cast<tree_element<Tag>*>(value), pos_to_ins.first._elem);
    }

    return iterator(static_cast<tree_element<Tag>*>(value));
  }

  iterator lower_bound(const Data_type& value) const {
    return get_bound(value, [](compare_res inp) { return inp != compare_res::less; });
  }

  iterator upper_bound(const Data_type& value) const {
    return get_bound(value, [](compare_res inp) { return inp == compare_res::greater; });
  }

  iterator find(const Data_type& value) const {
    auto res = find_to_insert(value);
    return (res.second == compare_res::equal) ? res.first : end();
  }

  bool operator==(const tree& other) const {
    auto iter_r = other.begin();
    for (auto iter_l = begin(); iter_l != end(); iter_l++) {
      if (compare_data(Get::get(*iter_r), Get::get(*iter_l)) != compare_res::equal) {
        return false;
      }
      iter_r++;
    }
    return true;
  }

  Node_type* get_node(iterator pos) {
    return static_cast<Node_type*>(pos._elem);
  }

  Node_type* erase(iterator pos) noexcept {
    Node_type* res = static_cast<Node_type*>(pos._elem);
    pos._elem->del_link();
    return res;
  }

  const Compare& get_comp() const {
    return comparator;
  }

private:
  [[no_unique_address]] Compare comparator;

  tree_element<Tag>* sentinel;

  std::pair<iterator, compare_res> find_to_insert(const Data_type& value) const {
    if (empty()) {
      return {end(), compare_res::less};
    }
    base_node* cur_node = sentinel->left_son;
    while (true) {
      compare_res temp_comp = compare_data(Get::get(*iterator(cur_node)), value);
      if (temp_comp == compare_res::less) {
        if (cur_node->right_son != cur_node) {
          cur_node = cur_node->right_son;
        } else {
          return {iterator(cur_node), compare_res::greater};
        }
      } else if (temp_comp == compare_res::equal) {
        return {iterator(cur_node), compare_res::equal};
      } else if (cur_node->left_son != cur_node) {
        cur_node = cur_node->left_son;
      } else {
        return {iterator(cur_node), compare_res::less};
      }
    }
  }

  iterator get_bound(const Data_type& value, std::function<bool(compare_res)> end_condition) const {
    for (auto iter = begin(); iter != end(); iter++) {
      if (end_condition(compare_data(Get::get(*iter), value))) {
        return iter;
      }
    }
    return end();
  }
};

} // namespace intrusive
