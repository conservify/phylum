#include "paging_delimited_buffer.h"

namespace phylum {

page_lock::~page_lock() {
    buffer_->release();
}

paging_delimited_buffer::paging_delimited_buffer(delimited_buffer &&other) : delimited_buffer(std::move(other)) {
}

page_lock paging_delimited_buffer::reading(dhara_sector_t sector) {
    phydebugf("page-lock: reading %d", sector);
    assert(!valid_);
    valid_ = true;
    return page_lock{ this };
}

page_lock paging_delimited_buffer::writing(dhara_sector_t sector) {
    phydebugf("page-lock: writing %d", sector);
    assert(!valid_);
    valid_ = true;
    return page_lock{ this };
}

void paging_delimited_buffer::ensure_valid() const {
    assert(valid_);
}

void paging_delimited_buffer::release() {
    phydebugf("page-lock: release");
    assert(valid_);
    valid_ = false;
}

} // namespace phylum
