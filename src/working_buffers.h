#pragma once

#include "simple_buffer.h"

namespace phylum {

class working_buffers {
protected:
    static constexpr size_t Size = 8;
    uint8_t *buffers_[Size];
    size_t buffer_size_{ 0 };
    size_t taken_[Size];
    size_t highwater_{ 0 };

public:
    working_buffers(size_t buffer_size) : buffer_size_(buffer_size) {
        for (auto i = 0u; i < Size; ++i) {
            buffers_[i] = nullptr;
            taken_[i] = 0u;
        }
    }

    virtual ~working_buffers() {
        for (auto i = 0u; i < Size; ++i) {
            taken_[i] = 0u;
        }
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
            if (buffers_[i] == nullptr) {
                buffers_[i] = ptr;
                taken_[i] = 0u;
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
            if (buffers_[i] == nullptr) break;
            if (taken_[i]) {
                hw++;
            }
        }

        if (hw > highwater_) {
            highwater_ = hw;
        }

        for (auto i = 0u; i < Size; ++i) {
            if (buffers_[i] == nullptr) {
                break;
            }
            if (!taken_[i]) {
                taken_[i] = true;
                phydebugf("wbuffers[%d]: allocate hw=%zu", i, highwater_);
                std::function<void(uint8_t*)> free_fn = std::bind(&working_buffers::free, this, std::placeholders::_1);
                return simple_buffer{ buffers_[i], size, free_fn };
            }
        }

        assert(false);
        return simple_buffer{ nullptr, 0u };
    }

    void free(uint8_t *ptr) {
        assert(ptr != nullptr);
        for (auto i = 0u; i < Size; ++i) {
            if (buffers_[i] == ptr) {
                phydebugf("wbuffers[%d]: free", i);
                taken_[i] = 0u;
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
