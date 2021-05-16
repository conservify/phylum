#include "flat_attribute_storage.h"
#include "data_chain.h"

namespace phylum {

struct attribute_header_t {
    uint8_t type{ 0xff };
    uint8_t size{ 0xff };
    uint8_t reserved[2] = { 0xff, 0xff };
};

flat_attribute_storage::flat_attribute_storage(phyctx pc) : buffers_(&pc.buffers_), sectors_(&pc.sectors_), allocator_(&pc.allocator_) {
}

int32_t flat_attribute_storage::read(tree_ptr_t &ptr, file_id_t /*id*/, open_file_config file_cfg) {
    data_chain chain{ pc(), head_tail_t{ ptr.root, ptr.tail } };

    phydebugf("reading attributes");

    while (true) {
        attribute_header_t header;

        auto err = chain.read((uint8_t *)&header, sizeof(attribute_header_t));
        if (err < 0) {
            return err;
        }
        if (err != sizeof(attribute_header_t)) {
            break;
        }

        phydebugf("attribute: type=%d size=%d", header.type, header.size);

        for (auto i = 0u; i < file_cfg.nattrs; ++i) {
            auto &attr = file_cfg.attributes[i];
            if (attr.type == header.type) {
                assert(attr.size == header.size);
                auto err = chain.read((uint8_t *)attr.ptr, attr.size);
                if (err < 0) {
                    return err;
                }
            }
        }
    }

    return 0;
}

int32_t flat_attribute_storage::update(tree_ptr_t &ptr, file_id_t /*id*/, open_file_attribute *attributes, size_t nattrs) {
    auto attribute_size = 0u;
    for (auto i = 0u; i < nattrs; ++i) {
        attribute_size += attributes[i].size;
    }

    assert(attribute_size <= AttributeCapacity);

    phydebugf("saving attributes total-size=%zu", attribute_size);

    data_chain chain{ pc(), head_tail_t{ ptr.root, ptr.tail } };

    if (ptr.valid()) {
        auto err = chain.truncate();
        if (err < 0) {
            return err;
        }
    }
    else {
        auto err = chain.create_if_necessary();
        if (err < 0) {
            return err;
        }

        ptr = tree_ptr_t{ chain.head(), chain.tail() };
    }

    auto wrote = 0u;

    for (auto i = 0u; i < nattrs; ++i) {
        auto &attr = attributes[i];

        attribute_header_t header;
        header.type = attr.type;
        header.size = attr.size;

        auto err = chain.write((uint8_t *)&header, sizeof(attribute_header_t));
        if (err < 0) {
            return err;
        }

        wrote += err;

        err = chain.write((uint8_t *)attr.ptr, attr.size);
        if (err < 0) {
            return err;
        }

        wrote += err;
    }

    phydebugf("saved %zu bytes", wrote);

    return wrote;
}

} // namespace phylum