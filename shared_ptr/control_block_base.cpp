#include <cb_details.h>
using namespace detail;
control_block::~control_block() = default;

void control_block::check_lifetime() {
  if (strong_ref_count + weak_ref_count == 0) {
    delete this;
  }
}

void control_block::inc_strong() {
  strong_ref_count++;
}

void control_block::inc_weak() {
  weak_ref_count++;
}

void control_block::dec_strong() {
  --strong_ref_count;
  if (strong_ref_count == 0) {
    clear();
  }
  check_lifetime();
}

void control_block::dec_weak() {
  --weak_ref_count;
  check_lifetime();
}

size_t control_block::get_str_ref_cnt() {
  return strong_ref_count;
}
