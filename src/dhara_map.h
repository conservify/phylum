#pragma once

#include <map>

#include "phylum.h"

namespace phylum {

class dhara_map {
private:
    size_t sector_size_{ 256 };
    std::map<dhara_sector_t, uint8_t *> map_;

public:
    dhara_map(size_t sector_size) : sector_size_(sector_size) {
    }

    virtual ~dhara_map() {
        clear();
    }

public:
    size_t sector_size() {
        return sector_size_;
    }

    dhara_sector_t size() {
        return map_.size();
    }

    int32_t write(dhara_sector_t sector, uint8_t const *data, size_t size) {
        assert(sector != UINT32_MAX);
        assert(size <= sector_size_);
        if (map_[sector] != nullptr) {
            free(map_[sector]);
        }
        map_[sector] = (uint8_t *)malloc(sector_size_);
        memcpy(map_[sector], data, size);
        debugf("dhara: write #%d\n", sector);
        // fk_dump_memory("write: ", data, size);
        return 0;
    }

    int32_t read(dhara_sector_t sector, uint8_t *data, size_t size) {
        assert(sector != UINT32_MAX);
        assert(size <= sector_size_);
        debugf("dhara: read #%d\n", sector);
        if (map_[sector] != nullptr) {
            memcpy(data, map_[sector], size);
            return 0;
        }
        return -1;
    }

    void clear() {
        for (auto const &e : map_) {
            free(e.second);
        }
        map_.clear();
    }
};

} // namespace phylum
