#pragma once

#include <cinttypes>
#include <cstdlib>
#include <cstring>

namespace phylum {

class task_stack {
private:
    size_t *stack_{ nullptr };
    int32_t size_{ 0 };
    int32_t position_{ 0 };
    char value_[256];

public:
    task_stack();

public:
    void push(const char *name);
    void pop();
    const char *get() {
        return value_;
    }
};

extern task_stack tasks;

class logged_task {
private:
    uint8_t pops_{ 0 };

public:
    logged_task(const char *name, const char *thing) {
        tasks.push(name);
        tasks.push(thing);
        pops_ = 2;
    }

    logged_task(const char *name) {
        tasks.push(name);
        pops_ = 1;
    }

    virtual ~logged_task() {
        while (pops_-- > 0) {
            tasks.pop();
        }
    }
};

} // namespace phylum
