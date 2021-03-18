#pragma once

#include "phylum.h"
#include "simple_buffer.h"

namespace phylum {

struct written_record {
    simple_buffer view;
    size_t size_of_record;
    size_t size_with_delimiter;

    template <typename T>
    T *as() {
        return reinterpret_cast<T *>(view.cursor());
    }

    template<typename T>
    simple_buffer data() {
        auto p = (uint8_t *)view.cursor() + sizeof(T);
        return simple_buffer{ p, size_of_record - sizeof(T) };
    }
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
    size_t offset_{ 0 };

public:
    delimited_buffer(size_t size, size_t offset = 0) : buffer_(size), offset_(offset) {
    }

    delimited_buffer(simple_buffer &&buffer, size_t offset = 0) : buffer_(std::move(buffer)), offset_(offset) {
    }

public:
    template <typename T>
    void append(T &record) {
        return append(&record, sizeof(T));
    }

    template <typename T>
    void append(T &record, uint8_t const *buffer, size_t size) {
        auto allocd = (uint8_t *)reserve(sizeof(T) + size);
        memcpy(allocd, &record, sizeof(T));
        memcpy(allocd + sizeof(T), buffer, size);
    }

    template <typename T, class... Args>
    void emplace(Args &&... args) {
        auto allocd = reserve(sizeof(T));
        new (allocd) T(std::forward<Args>(args)...);
    }

    void append(void const *source, size_t length) {
        memcpy(reserve(length), source, length);
    }

    int32_t terminate() {
        (uint8_t *)reserve(0);
        return position();
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
    int32_t unsafe_at(T fn) {
        return buffer_.unsafe_at<T>(fn);
    }

    simple_buffer &read_view() {
        return buffer_;
    }

    simple_buffer &write_view() {
        return buffer_;
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
    void *reserve(size_t length);

public:
    class iterator {
    private:
        simple_buffer view_;
        written_record record_;

    public:
        uint8_t *ptr() {
            return view_.ptr();
        }

        size_t position() {
            return view_.position();
        }

    public:
        iterator(simple_buffer &&view) : view_(std::move(view)) {
            if (view_.valid()) {
                auto offset = 0u;
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

            auto maybe_record_length = 0u;
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
            record_.view = view_;
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
    iterator begin() const {
        return iterator(buffer_.begin_view());
    }

    iterator end() const {
        return iterator(buffer_.end_view());
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

} // namespace phylum
