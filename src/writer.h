#pragma once

#include "phylum.h"
#include "simple_buffer.h"
#include "blake2b.h"

namespace phylum {

class io_writer {
public:
    virtual int32_t write(uint8_t const *data, size_t size) = 0;

    int32_t write(char const *str) {
        return write((uint8_t *)str, strlen(str));
    }

    int32_t write(char const *str, size_t size) {
        return write((uint8_t *)str, size);
    }

};

class buffer_writer : public io_writer {
private:
    write_buffer &buffer_;

public:
    buffer_writer(write_buffer &buffer) : buffer_(buffer) {
    }

public:
    int32_t write(uint8_t const *data, size_t size) override {
        return buffer_.fill(data, size, [](write_buffer&) -> int32_t {
            // TODO Overflow
            assert(0);
            return -1;
        });
    }

};

class noop_writer : public io_writer {
public:
    int32_t write(uint8_t const */*data*/, size_t size) override {
        return size;
    }

};

constexpr size_t HashSize = 32;

class blake2b_writer : public io_writer {
private:
    io_writer *target_{ nullptr };
    BLAKE2b b2b_;

public:
    blake2b_writer(io_writer *target);

public:
    int32_t write(uint8_t const *data, size_t size) override;

public:
    void finalize(void *hash, size_t buffer_length);

};

} // namespace phylum
