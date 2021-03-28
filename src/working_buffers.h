#pragma once

#include <simple_buffer.h>

namespace phylum {

template<size_t Size>
class working_buffers {
protected:
    uint8_t *buffers_[Size];
    size_t buffer_size_{ 0 };
    size_t taken_[Size];

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
        phydebugf("wbuffers: allocate");
        assert(size == buffer_size_);

        for (auto i = 0u; i < Size; ++i) {
            if (buffers_[i] == nullptr) {
                break;
            }
            if (!taken_[i]) {
                taken_[i] = true;
                std::function<void(uint8_t*)> free_fn = std::bind(&working_buffers::free, this, std::placeholders::_1);
                return simple_buffer{ buffers_[i], size, free_fn };
            }
        }

        assert(false);
        return simple_buffer{ nullptr, 0u };
    }

    void free(uint8_t *ptr) {
        phydebugf("wbuffers: free");
        assert(ptr != nullptr);
        for (auto i = 0u; i < Size; ++i) {
            if (buffers_[i] == ptr) {
                taken_[i] = 0u;
                break;
            }
        }
    }

};

template<size_t Size>
class malloc_working_buffers : public working_buffers<Size> {
private:
    uint8_t *memory_{ nullptr };

public:
    malloc_working_buffers(size_t buffer_size) : working_buffers<Size>(buffer_size) {
        memory_ = (uint8_t *)malloc(Size * buffer_size);
        for (auto i = 0u; i < Size; ++i) {
            this->lend(memory_ + (i * buffer_size), buffer_size);
        }
    }

    virtual ~malloc_working_buffers() {
        free(memory_);
    }
};

} // namespace phylum
