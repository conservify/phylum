#pragma once

#include "phylum.h"

namespace phylum {

class io_writer {
public:
    virtual int32_t write(uint8_t const *data, size_t size) = 0;

    int32_t write(char const *str) {
        return write((uint8_t *)str, strlen(str));
    }

    int32_t write(char const *str, size_t size) {
        return write((uint8_t *)str, size);
    }

};

} // namespace phylum
