#pragma once

#include <algorithm>
#include <cassert>
#include <cinttypes>
#include <cstdarg>
#include <cstring>
#include <cstdio>

namespace phylum {

static inline int32_t phy_vsnprintf(char *buffer, size_t size, const char *f, ...) {
    va_list args;
    va_start(args, f);
    auto err = vsnprintf(buffer, size, f, args);
    va_end(args);
    return err;
}

uint32_t crc32_checksum(const char *str);

int32_t debugf(const char *f, ...);

void fk_dump_memory(const char *prefix, uint8_t const *p, size_t size, ...);

template <class T, class U = T> T exchange(T &obj, U &&new_value) {
    T old_value = std::move(obj);
    obj = std::forward<U>(new_value);
    return old_value;
}

} // namespace phylum

#include "entries.h"
