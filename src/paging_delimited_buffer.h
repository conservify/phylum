#pragma once

#include "delimited_buffer.h"
#include "sector_allocator.h"
#include "working_buffers.h"

namespace phylum {

namespace waiting {

class page_lock {
private:
    working_buffers &buffers_;
    delimited_buffer &buffer_;
    dhara_sector_t sector_{ InvalidSector };
    bool read_only_{ true };

public:
    page_lock(working_buffers &buffers, delimited_buffer &buffer, dhara_sector_t sector)
        : buffers_(buffers), buffer_(buffer), sector_(sector) {
    }

    page_lock(working_buffers &buffers, delimited_buffer &buffer, dhara_sector_t sector, bool read_only)
        : buffers_(buffers), buffer_(buffer), sector_(sector), read_only_(read_only) {
    }

    virtual ~page_lock() {
    }
};

}; // namespace waiting

class paging_delimited_buffer;

class page_lock {
private:
    paging_delimited_buffer *buffer_{ nullptr };

public:
    page_lock(paging_delimited_buffer *buffer) : buffer_(buffer) {
    }

    virtual ~page_lock();
};

class paging_delimited_buffer : public delimited_buffer {
private:
    bool valid_{ false };

public:
    paging_delimited_buffer(delimited_buffer &&other);

public:
    page_lock reading(dhara_sector_t sector);

    page_lock writing(dhara_sector_t sector);

protected:
    void ensure_valid() const override;

    void release();

    friend class page_lock;
};

} // namespace phylum
