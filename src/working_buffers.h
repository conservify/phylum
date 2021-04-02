#pragma once

#include "simple_buffer.h"

namespace phylum {

class working_buffers {
protected:
    struct page_t {
        uint8_t *buffer{ nullptr };
        size_t size{ 0 };
        dhara_sector_t sector{ InvalidSector };
        int32_t refs{ 0 };
    };

    static constexpr size_t Size = 8;
    size_t buffer_size_{ 0 };
    page_t pages_[Size];
    size_t highwater_{ 0 };

public:
    working_buffers(size_t buffer_size) : buffer_size_(buffer_size) {
        for (auto i = 0u; i < Size; ++i) {
            pages_[i] = { };
        }
    }

    virtual ~working_buffers() {
        phyinfof("wbuffers::dtor hw=%zu", highwater_);
    }

public:
    size_t buffer_size() {
        return buffer_size_;
    }

public:
    void lend(uint8_t *ptr, size_t size) {
        assert(buffer_size_ == size);
        for (auto i = 0u; i < Size; ++i) {
            if (pages_[i].buffer == nullptr) {
                pages_[i].buffer = ptr;
                pages_[i].size = size;
                break;
            }
        }
    }

public:
    simple_buffer allocate(size_t size) {
        assert(size == buffer_size_);

        auto hw = 1u; // We're going to increase this sum by 1 after
                      // this function ends or fail horribly.
        for (auto i = 0u; i < Size; ++i) {
            if (pages_[i].buffer == nullptr) break;
            if (pages_[i].refs > 0) {
                hw++;
            }
        }

        if (hw > highwater_) {
            highwater_ = hw;
        }

        for (auto i = 0u; i < Size; ++i) {
            if (pages_[i].buffer == nullptr) {
                break;
            }
            if (pages_[i].refs == 0) {
                pages_[i].refs++;
                phydebugf("wbuffers[%d]: allocate hw=%zu", i, highwater_);
                std::function<void(uint8_t const *)> free_fn = std::bind(&working_buffers::free, this, std::placeholders::_1);
                return simple_buffer{ pages_[i].buffer, size, free_fn };
            }
        }

        assert(false);
        return simple_buffer{ nullptr, 0u };
    }

    void free(uint8_t const *ptr) {
        assert(ptr != nullptr);
        for (auto i = 0u; i < Size; ++i) {
            if (pages_[i].buffer == ptr) {
                pages_[i].refs--;

                if (pages_[i].refs == 0) {
                    pages_[i].sector = InvalidSector;
                }
                phydebugf("wbuffers[%d]: free refs=%d", i, pages_[i].refs);
                break;
            }
        }
    }

};

class malloc_working_buffers : public working_buffers {
private:
    uint8_t *memory_{ nullptr };

public:
    malloc_working_buffers(size_t buffer_size) : working_buffers(buffer_size) {
        memory_ = (uint8_t *)malloc(Size * buffer_size);
        memset(memory_, 0xff, Size * buffer_size);
        for (auto i = 0u; i < Size; ++i) {
            this->lend(memory_ + (i * buffer_size), buffer_size);
        }
    }

    virtual ~malloc_working_buffers() {
        ::free(memory_);
    }
};

} // namespace phylum
