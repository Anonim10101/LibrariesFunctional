#include "intrusive-list.h"
using namespace intrusive;

base_node::base_node() : prev(this), next(this) {}

base_node::~base_node() {
  del_link();
}

base_node::base_node(base_node&& other) {
  other.next->link_back(this);
  link_back(other.prev);
  other.unlink();
}

base_node& base_node::operator=(base_node&& other) {
  if (this != &other) {
    this->del_link();
    other.next->link_back(this);
    link_back(other.prev);
    other.unlink();
  }
  return *this;
}

base_node::base_node(const base_node&) {}

base_node& base_node::operator=(const base_node& other) {
  if (this != &other) {
    del_link();
  }
  return *this;
}

void base_node::del_link() {
  next->link_back(prev);
  unlink();
}

void base_node::unlink() noexcept {
  prev = this;
  next = this;
}

void base_node::link_back(base_node* other) {
  other->next = this;
  this->prev = other;
}

base_node* base_node::get_next() const {
  return next;
}

base_node* base_node::get_prev() const {
  return prev;
}
