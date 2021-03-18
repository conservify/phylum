#pragma once

#include "dhara_map.h"

namespace phylum {

class sector_allocator {
private:
    dhara_map &dhara_;
    dhara_sector_t counter_{ 0 };

public:
    sector_allocator(dhara_map &dhara) : dhara_(dhara) {
        counter_ = dhara_.size() + 1;
    }

public:
    dhara_sector_t allocate() {
        // TODO We should also maintain a free sector collection. We
        // can do this by just keeping a sector chain with their numbers.
        return counter_++;
    }
};

} // namespace phylum
