#pragma once

#include "phylum.h"

namespace phylum {

class dhara_map {
public:
    virtual ~dhara_map() { }

public:
    virtual size_t sector_size() = 0;
    virtual dhara_sector_t size() = 0;
    virtual int32_t write(dhara_sector_t sector, uint8_t const *data, size_t size) = 0;
    virtual int32_t read(dhara_sector_t sector, uint8_t *data, size_t size) = 0;
    virtual int32_t clear() = 0;
};

} // namespace phylum
