#pragma once

#include "data_chain.h"
#include "directory_chain.h"
#include "simple_buffer.h"

namespace phylum {

class file_reader {
private:
    directory_chain &directory_;
    found_file file_;
    simple_buffer buffer_;
    data_chain data_chain_;

public:
    file_reader(directory_chain &directory, found_file file, simple_buffer &&buffer);

    virtual ~file_reader();

public:
    int32_t read(uint8_t *data, size_t size);

    int32_t close();

    uint32_t u32(uint8_t type);

private:
    bool has_chain() {
        return data_chain_.valid();
    }
};

} // namespace phylum
