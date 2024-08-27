#pragma once

#include "intrusive-list.h"

#include <functional>
#include <list>

namespace signals {

template <typename T>
struct signal;

template <typename... Args>
struct signal<void(Args...)> {
  using slot_t = std::function<void(Args...)>;

  struct connection : intrusive::list_element<struct con_tag> {
    friend struct signal;
    connection() = default;

    connection(connection&& other) : call(std::move(other.call)), origin(other.origin) {
      change_lists(other);
    }

    connection& operator=(connection&& other) {
      if (this != &other) {
        disconnect();
        origin = other.origin;
        call = std::move(other.call);
        change_lists(other);
      }
      return *this;
    }

    void disconnect() noexcept {
      if (origin != nullptr) {
        for (iterator_token* copy = origin->tail; copy != nullptr; copy = copy->prev) {
          if (this == &*copy->it) {
            copy->it++;
          }
        }
        erase_from_origin();
      }
    }

    ~connection() {
      disconnect();
    }

  private:
    connection(signal* sig, slot_t slot) : call(std::move(slot)), origin(sig) {
      sig->_slots.push_back(*this);
    }

    void erase_from_origin() {
      if (origin != nullptr) {
        origin->_slots.erase(origin->_slots.get_iterator(*this));
        call = slot_t();
        origin = nullptr;
      }
    }

    void change_lists(connection& other) {
      if (other.origin != nullptr) {
        origin->_slots.insert(++origin->_slots.get_iterator(other), *this);
        other.disconnect();
      }
    }

    slot_t call;
    signal* origin = nullptr;
  };

  signal() = default;
  signal(const signal&) = delete;
  signal& operator=(const signal&) = delete;

  ~signal() {
    for (iterator_token* token = tail; token != nullptr; token = token->prev) {
      token->origin = nullptr;
    }
    while (!_slots.empty()) {
      _slots.begin()->erase_from_origin();
    }
  }

  connection connect(std::function<void(Args...)> slot) noexcept {
    return connection(this, std::move(slot));
  }

  void operator()(Args... args) const {
    iterator_token tok(this);
    while (tok.it != _slots.end()) {
      auto copy = tok.it++;
      (copy->call)(std::forward<Args>(args)...);
      if (tok.origin == nullptr) {
        return;
      }
    }
  }

private:
  struct iterator_token {
    iterator_token(const signal* o) : origin(o), prev(origin->tail), it(origin->_slots.begin()) {
      origin->tail = this;
    }

    ~iterator_token() {
      if (origin != nullptr) {
        origin->tail = prev;
      }
    }

    const signal* origin = nullptr;
    iterator_token* prev = nullptr;
    typename intrusive::list<connection, struct con_tag>::const_iterator it;
  };
  intrusive::list<connection, struct con_tag> _slots;
  mutable iterator_token* tail = nullptr;
};

} // namespace signals
