#pragma once

#include <map>

#include "dhara_map.h"

namespace phylum {

class memory_dhara_map : public dhara_map {
private:
    size_t sector_size_{ 256 };
    std::map<dhara_sector_t, uint8_t *> map_;

public:
    memory_dhara_map(size_t sector_size) : sector_size_(sector_size) {
    }

    virtual ~memory_dhara_map() {
        clear();
    }

public:
    size_t sector_size() override {
        return sector_size_;
    }

    dhara_sector_t size() override {
        return map_.size();
    }

    int32_t write(dhara_sector_t sector, uint8_t const *data, size_t size) override {
        assert(sector != UINT32_MAX);
        assert(size <= sector_size_);
        if (map_[sector] != nullptr) {
            free(map_[sector]);
        }
        map_[sector] = (uint8_t *)malloc(sector_size_);
        memcpy(map_[sector], data, size);
        phydebugf("dhara: write #%d", sector);
        return 0;
    }

    int32_t read(dhara_sector_t sector, uint8_t *data, size_t size) override {
        assert(sector != UINT32_MAX);
        assert(size <= sector_size_);
        phydebugf("dhara: read #%d", sector);
        if (map_[sector] != nullptr) {
            memcpy(data, map_[sector], size);
            return 0;
        }
        return -1;
    }

    int32_t clear() override {
        for (auto const &e : map_) {
            free(e.second);
        }
        map_.clear();
        return 0;
    }
};

}