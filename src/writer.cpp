#include "writer.h"

namespace phylum {

blake2b_writer::blake2b_writer(io_writer *target) : target_(target) {
    b2b_.reset(HashSize);
}

int32_t blake2b_writer::write(uint8_t const *data, size_t size) {
    auto err = target_->write(data, size);
    if (err <= 0) {
        return err;
    }
    b2b_.update(data, size);
    return err;
}

void blake2b_writer::finalize(void *hash, size_t buffer_length) {
    b2b_.finalize(hash, buffer_length);
}

} // namespace phylum
