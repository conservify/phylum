#pragma once

#include "phylum.h"

namespace phylum {

class io_reader {
public:
    virtual int32_t read(uint8_t *data, size_t size) = 0;

    virtual int32_t read(size_t size) {
        return read(nullptr, size);
    }

};

} // namespace phylum
