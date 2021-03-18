#include "task_stack.h"

namespace phylum {

task_stack::task_stack() {
    memset(value_, 0x00, sizeof(value_));
    size_ = 10;
    stack_ = (size_t *)malloc(sizeof(size_t) * size_);
}

void task_stack::push(const char *name) {
    if (position_ < size_) {
        auto value_len = strlen(value_);
        auto name_len = strlen(name);
        stack_[position_] = value_len;
        value_[value_len] = ' ';
        memcpy(value_ + value_len + 1, name, name_len);
        value_[value_len + name_len + 1] = 0;
        position_++;
    }
}

void task_stack::pop() {
    position_--;

    auto old_len = stack_[position_];
    memset(value_ + old_len, 0x00, sizeof(value_) - old_len);
}

} // namespace phylum
