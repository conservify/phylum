#pragma once

#include "phylum.h"
#include "simple_buffer.h"

namespace phylum {

class delimited_buffer;

struct written_record {
    delimited_buffer *db{ nullptr };
    simple_buffer view;
    size_t size_of_record;
    size_t size_with_delimiter;
    size_t position;

    template <typename T>
    T *as() {
        return reinterpret_cast<T *>(view.cursor());
    }

    template<typename DataType, typename T>
    int32_t data(T fn);

    template<typename DataType, typename T>
    int32_t read_data(T fn);
};

struct const_written_record {
    delimited_buffer const *db{ nullptr };
    simple_buffer view;
    size_t size_of_record;
    size_t size_with_delimiter;
    size_t position;

    template <typename T>
    T *as() {
        return reinterpret_cast<T *>(view.cursor());
    }

    template<typename DataType, typename T>
    int32_t data(T fn);
};

/**
 * This has the mechanics to allow us to overlap records across sector
 * boundaries. I'm torn on introducing this complexity though because
 * what happens is changing those records becomes a multiblock
 * juggle. If your flash memory has small pages relative to the data
 * being stored this could be a problem.
 *
 * This may be acceptable for certain kinds of records, though and
 * that may be worth the effort. For example, some things will never
 * be modified.
 */
class delimited_buffer {
private:
    simple_buffer buffer_;

public:
    delimited_buffer(simple_buffer &&buffer) : buffer_(std::move(buffer)) {
    }

    delimited_buffer(delimited_buffer &&other)
        : buffer_(std::exchange(other.buffer_, simple_buffer{ })) {
    }

    virtual ~delimited_buffer() {
    }

public:
    template <typename T>
    sector_offset_t append(T &record) {
        return append(&record, sizeof(T));
    }

    template <typename T>
    sector_offset_t append(T &record, uint8_t const *buffer, size_t size) {
        sector_offset_t start_position{ 0 };
        auto allocd = (uint8_t *)reserve(sizeof(T) + size, start_position);
        memcpy(allocd, &record, sizeof(T));
        memcpy(allocd + sizeof(T), buffer, size);
        return start_position;
    }

    template <typename T, class... Args>
    sector_offset_t emplace(Args &&... args) {
        sector_offset_t start_position{ 0 };
        auto allocd = reserve(sizeof(T), start_position);
        new (allocd) T(std::forward<Args>(args)...);
        return start_position;
    }

    sector_offset_t append(void const *source, size_t length) {
        sector_offset_t start_position{ 0 };
        memcpy(reserve(length, start_position), source, length);
        return start_position;
    }

    template<typename T>
    struct appended_record {
        sector_offset_t position;
        T *record;
    };

    template<typename T>
    appended_record<T> reserve() {
        sector_offset_t start_position{ 0 };
        auto size = sizeof(T);
        auto alloc = reserve(size, start_position);
        bzero(alloc, size);
        auto ptr = new (alloc) T();
        return appended_record<T>{ start_position, ptr };
    }

    /**
     * This reserves 0 bytes, which ends up inserting a 0 byte length
     * and that acts a NULL terminator.
     */
    sector_offset_t terminate() {
        sector_offset_t start_position{ 0 };
        (uint8_t *)reserve(0, start_position);
        return start_position;
    }

    template <typename T>
    bool room_for() {
        return room_for(sizeof(T));
    }

    bool room_for(size_t length) {
        return buffer_.room_for(varint_encoding_length(length) + length);
    }

    template <typename T>
    int32_t unsafe_all(T fn) {
        return buffer_.unsafe_all<T>(fn);
    }

    template <typename T>
    int32_t unsafe_forever(T fn) {
        return buffer_.unsafe_forever<T>(fn);
    }

    template <typename T>
    int32_t read_to_end(T fn) {
        return buffer_.read_to_end<T>(fn);
    }

    template <typename T>
    int32_t read_to_position(T fn) {
        return buffer_.read_to_position<T>(fn);
    }

    read_buffer to_read_buffer() {
        return read_buffer{ buffer_.ptr(), buffer_.size(), buffer_.position() };
    }

    template <typename T>
    T const *header() const {
        return begin()->as<T>();
    }

    template<typename THeader>
    int32_t write_header(std::function<int32_t(THeader *header)> fn) {
        return unsafe_all([&](uint8_t */*ptr*/, size_t /*size*/) {
            return fn(begin()->as<THeader>());
        });
    }

    int32_t write_view(std::function<int32_t(write_buffer)> fn) {
        return unsafe_all([&](uint8_t *ptr, size_t size) {
            return fn(write_buffer(ptr, size, position()));
        });
    }

    int32_t skip_end() {
        return buffer_.skip_end();
    }

    int32_t skip(size_t bytes) {
        return buffer_.skip(bytes);
    }

    int32_t constrain(size_t bytes) {
        return buffer_.constrain(bytes);
    }

private:
    void *reserve(size_t length, sector_offset_t &start_position);

public:
    class const_iterator {
    private:
        delimited_buffer const *db_{ nullptr };
        simple_buffer view_;
        const_written_record record_;

    public:
        size_t position() {
            return view_.position();
        }

    public:
        const_iterator(delimited_buffer const *db, simple_buffer &&view) : db_(db), view_(std::move(view)) {
            if (view_.valid()) {
                uint32_t offset = 0u;
                if (!view_.try_read(offset)) {
                    view_ = view_.end_view();
                } else {
                    if (!read()) {
                        view_ = view_.end_view();
                    }
                }
            }
        }

    private:
        bool read() {
            assert(view_.valid());

            // Position of the delimiter, rather than the record.
            auto record_position = position();

            uint32_t maybe_record_length = 0u;
            if (!view_.try_read(maybe_record_length)) {
                record_ = const_written_record{};
                return false;
            }

            if (maybe_record_length == 0) {
                record_ = const_written_record{};
                return false;
            }

            auto delimiter_overhead = varint_encoding_length(maybe_record_length);

            // Clear record and fill in the details we have.
            record_ = const_written_record{};
            record_.db = db_;
            record_.view = view_;
            record_.position = record_position;
            record_.size_of_record = maybe_record_length;
            record_.size_with_delimiter = maybe_record_length + delimiter_overhead;

            return true;
        }

    public:
        const_iterator &operator++() {
            // Advance to the record after this one, we only adjust
            // position_, then we do a read and see if we have a
            // legitimate record length.
            view_.skip(record_.size_of_record);
            if (!view_.valid()) {
                view_ = view_.end_view();
                return *this;
            }
            if (!read()) {
                view_ = view_.end_view();
            }
            return *this;
        }

        bool operator!=(const const_iterator &other) const {
            return view_.position() != other.view_.position();
        }

        bool operator==(const const_iterator &other) const {
            return view_.position() == other.view_.position();
        }

        const_written_record &operator*() {
            return record_;
        }

        const_written_record *operator->() {
            return &record_;
        }
    };

    class iterator {
    private:
        delimited_buffer *db_{ nullptr };
        simple_buffer view_;
        written_record record_;

    public:
        size_t position() {
            return view_.position();
        }

    public:
        iterator(delimited_buffer *db, simple_buffer &&view) : db_(db), view_(std::move(view)) {
            if (view_.valid()) {
                uint32_t offset = 0u;
                if (!view_.try_read(offset)) {
                    view_ = view_.end_view();
                } else {
                    if (!read()) {
                        view_ = view_.end_view();
                    }
                }
            }
        }

    private:
        bool read() {
            assert(view_.valid());

            // Position of the delimiter, rather than the record.
            auto record_position = position();

            uint32_t maybe_record_length = 0u;
            if (!view_.try_read(maybe_record_length)) {
                record_ = written_record{};
                return false;
            }

            if (maybe_record_length == 0) {
                record_ = written_record{};
                return false;
            }

            auto delimiter_overhead = varint_encoding_length(maybe_record_length);

            // Clear record and fill in the details we have.
            record_ = written_record{};
            record_.db = db_;
            record_.view = view_;
            record_.position = record_position;
            record_.size_of_record = maybe_record_length;
            record_.size_with_delimiter = maybe_record_length + delimiter_overhead;

            return true;
        }

    public:
        iterator &operator++() {
            // Advance to the record after this one, we only adjust
            // position_, then we do a read and see if we have a
            // legitimate record length.
            view_.skip(record_.size_of_record);
            if (!view_.valid()) {
                view_ = view_.end_view();
                return *this;
            }
            if (!read()) {
                view_ = view_.end_view();
            }
            return *this;
        }

        bool operator!=(const iterator &other) const {
            return view_.position() != other.view_.position();
        }

        bool operator==(const iterator &other) const {
            return view_.position() == other.view_.position();
        }

        written_record &operator*() {
            return record_;
        }

        written_record *operator->() {
            return &record_;
        }
    };

public:
    iterator begin() {
        return iterator(this, buffer_.begin_view());
    }

    iterator end() {
        return iterator(this, buffer_.end_view());
    }

    const_iterator begin() const {
        return const_iterator(this, buffer_.begin_view());
    }

    const_iterator end() const {
        return const_iterator(this, buffer_.end_view());
    }

    int32_t seek_end() {
        auto iter = begin();
        while (iter != end()) {
            buffer_.position(iter.position() + iter->size_of_record);
            iter = ++iter;
        }
        return 0;
    }

    int32_t seek_once() {
        auto iter = begin();
        while (iter != end()) {
            buffer_.position(iter.position() + iter->size_of_record);
            return 0;
        }
        return -1;
    }

    size_t size() const {
        return buffer_.size();
    }

    size_t position() const {
        return buffer_.position();
    }

    bool empty() const {
        return buffer_.position() == 0;
    }

    bool at_start() const {
        return empty();
    }

    void position(size_t position) {
        buffer_.position(position);
    }

    size_t available() const {
        return buffer_.available();
    }

    void clear(uint8_t value = 0xff) {
        buffer_.clear(value);
    }

    void rewind() {
        buffer_.rewind();
    }
};

template <typename DataType, typename T>
int32_t written_record::data(T fn) {
    assert(db != nullptr);
    return db->unsafe_all([&](uint8_t */*ptr*/, size_t /*size*/) -> int32_t {
        return fn((uint8_t *)view.cursor() + sizeof(DataType), size_of_record - sizeof(DataType));
    });
}

template<typename DataType, typename T>
int32_t written_record::read_data(T fn) {
    assert(db != nullptr);
    return db->read_to_end([&](read_buffer all_buffer) -> int32_t {
        read_buffer data_buffer(all_buffer.ptr() + sizeof(DataType), size_of_record - sizeof(DataType));
        return fn(std::move(data_buffer));
    });
}

} // namespace phylum
