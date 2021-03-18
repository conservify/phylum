#pragma once

#include <algorithm>
#include <cassert>
#include <cinttypes>
#include <cstdarg>
#include <cstring>

namespace phylum {

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
