#include "file_reader.h"

namespace phylum {

file_reader::file_reader(directory_chain &directory, found_file file, simple_buffer &&buffer)
    : directory_(directory), file_(file), buffer_(std::move(buffer)), data_chain_(directory, file.chain) {
}

file_reader::~file_reader() {
}

int32_t file_reader::read(uint8_t */*data*/, size_t /*size*/) {
    return -1;
}

int32_t file_reader::close() {
    return -1;
}

uint32_t file_reader::u32(uint8_t /*type*/) {
    return -1;
}

} // namespace phylum
