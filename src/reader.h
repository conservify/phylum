#pragma once

#include "phylum.h"

namespace phylum {

class io_reader {
public:
    virtual int32_t read(uint8_t *data, size_t size) = 0;

    virtual int32_t read(size_t size) {
        return read(nullptr, size);
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

} // namespace phylum
