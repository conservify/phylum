#pragma once

#include "writer.h"
#include "reader.h"
#include "simple_buffer.h"

namespace phylum {

class cobs_writer : public io_writer {
private:
    io_writer *target_{ nullptr };
    write_buffer &buffer_;

public:
    cobs_writer(io_writer *target, write_buffer &buffer);
    virtual ~cobs_writer();

public:
    int32_t write(uint8_t const *data, size_t size) override;

};

class cobs_reader : public io_reader {
private:
    io_reader *target_{ nullptr };

public:
    cobs_reader(io_reader *target);

public:
    int32_t read(uint8_t *data, size_t size) override;

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

class buffer_reader : public io_reader {
private:
    read_buffer buffer_;

public:
    buffer_reader(read_buffer &&buffer) : buffer_(std::move(buffer)) {
    }

public:
    int32_t read(uint8_t *data, size_t size) {
        write_buffer copying{ data, size };
        return copying.fill_from(buffer_);
    }

};

class noop_writer : public io_writer {
public:
    int32_t write(uint8_t const */*data*/, size_t size) override {
        return size;
    }

};

} // namespace phylum
