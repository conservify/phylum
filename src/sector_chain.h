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

class lazy_delimited_buffer;

class page_lock {
private:
    lazy_delimited_buffer *buffer_{ nullptr };

public:
    page_lock(lazy_delimited_buffer *buffer) : buffer_(buffer) {
    }

    virtual ~page_lock();
};

class lazy_delimited_buffer : public delimited_buffer {
private:
    bool valid_{ false };

public:
    lazy_delimited_buffer(delimited_buffer &&other) : delimited_buffer(std::move(other)) {
    }

public:
    page_lock reading(dhara_sector_t sector) {
        phydebugf("page-lock: reading %d", sector);
        assert(!valid_);
        valid_ = true;
        return page_lock{ this };
    }

    page_lock writing(dhara_sector_t sector) {
        phydebugf("page-lock: writing %d", sector);
        assert(!valid_);
        valid_ = true;
        return page_lock{ this };
    }

protected:
    void ensure_valid() const override {
        assert(valid_);
    }

    void release();

    friend class page_lock;
};

class sector_chain {
private:
    static constexpr size_t ChainNameLength = 32;

private:
    using buffer_type = lazy_delimited_buffer;
    working_buffers *buffers_{ nullptr };
    sector_map *sectors_{ nullptr };
    sector_allocator *allocator_{ nullptr };
    buffer_type buffer_;
    dhara_sector_t head_{ InvalidSector };
    dhara_sector_t tail_{ InvalidSector };
    dhara_sector_t sector_{ InvalidSector };
    dhara_sector_t length_sectors_{ 0 };
    bool dirty_{ false };
    bool appendable_{ false };
    const char *prefix_{ "sector-chain" };
    char name_[ChainNameLength];

public:
    sector_chain(working_buffers &buffers, sector_map &sectors, sector_allocator &allocator, head_tail_t chain, const char *prefix)
        : buffers_(&buffers), sectors_(&sectors), allocator_(&allocator), buffer_(std::move(buffers.allocate(sectors.sector_size()))), head_(chain.head),
          tail_(chain.tail), prefix_(prefix) {
        name("%s[unk]", prefix_);
    }

    sector_chain(sector_chain &other, head_tail_t chain, const char *prefix)
        : buffers_(other.buffers_), sectors_(other.sectors_), allocator_(other.allocator_),
          buffer_(std::move(buffers().allocate(other.sector_size()))), head_(chain.head), tail_(chain.tail), prefix_(prefix) {
        name("%s[unk]", prefix_);
    }

    virtual ~sector_chain() {
    }

public:
    working_buffers &buffers() {
        return *buffers_;
    }

    dhara_sector_t length_sectors() const {
        return length_sectors_;
    }

    bool valid() const {
        return head_ != 0 && head_ != InvalidSector && tail_ != 0 && tail_ != InvalidSector;
    }

    dhara_sector_t head() const {
        return head_;
    }

    dhara_sector_t tail() const {
        return tail_;
    }

    int32_t log();

    int32_t create_if_necessary();

    const char *name() const {
        return name_;
    }

    int32_t flush();

protected:
    int32_t flush(page_lock &page_lock);

    void name(const char *f, ...);

    dhara_sector_t sector() const {
        return sector_;
    }

    sector_map *sectors() {
        return sectors_;
    }

    void dirty(bool value) {
        dirty_ = value;
    }

    bool dirty() {
        return dirty_;
    }

    void appendable(bool value) {
        appendable_ = value;
    }

    bool appendable() {
        return appendable_;
    }

    sector_allocator &allocator() {
        return *allocator_;
    }

    buffer_type &db() {
        return buffer_;
    }

    size_t sector_size() const {
        return buffer_.size();
    }

    void head(dhara_sector_t sector) {
        head_ = sector;
        if (tail_ == InvalidSector) {
            tail(sector);
        }
    }

    void tail(dhara_sector_t sector) {
        tail_ = sector;
        if (head_ == InvalidSector) {
            head(sector);
        }
    }

    void sector(dhara_sector_t sector) {
        sector_ = sector;
        name("%s[%d]", prefix_, sector_);
    }

    int32_t assert_valid() {
        assert(head_ != InvalidSector);
        return 0;
    }

    int32_t ensure_loaded(page_lock &page_lock) {
        assert_valid();

        if (sector_ == InvalidSector) {
            return forward(page_lock);
        }

        return 0;
    }

    int32_t back_to_head();

    int32_t back_to_tail();

    int32_t forward(page_lock &page_lock);

    template <typename T>
    int32_t walk(T fn) {
        logged_task lt{ "sc-walk", name() };

        assert_valid();

        assert(!dirty());

        auto page_lock = db().reading(head());

        assert(back_to_head() >= 0);

        while (true) {
            auto err = forward(page_lock);
            if (err < 0) {
                return err;
            }
            if (err == 0) {
                break;
            }
            for (auto &record_ptr : buffer_) {
                auto entry = record_ptr.as<entry_t>();
                assert(entry != nullptr);
                auto err = fn(entry, record_ptr);
                if (err < 0) {
                    return err;
                }
                if (err > 0) {
                    return err;
                }
            }
        }

        return 0;
    }

    int32_t load(page_lock &page_lock);

    int32_t grow_tail(page_lock &page_lock);

    int32_t write_header_if_at_start(page_lock &page_lock);

    virtual int32_t seek_end_of_chain(page_lock &page_lock);

    virtual int32_t seek_end_of_buffer(page_lock &page_lock) = 0;

    virtual int32_t write_header(page_lock &page_lock) = 0;
};

} // namespace phylum
