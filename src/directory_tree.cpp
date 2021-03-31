#include "directory_tree.h"

namespace phylum {

int32_t directory_tree::mount() {
    if (!tree_.exists()) {
        return -1;
    }

    return 0;
}

int32_t directory_tree::format() {
    return tree_.create();
}

int32_t directory_tree::touch(const char *name) {
    auto id = make_file_id(name);

    value_type entry;
    auto err = tree_.add(id, entry);
    if (err < 0) {
        return err;
    }

    return 0;
}

int32_t directory_tree::find(const char *name, open_file_config file_cfg) {
    auto id = make_file_id(name);

    file_ = found_file{};
    file_.cfg = file_cfg;

    // Zero attribute values before we scan.
    for (auto i = 0u; i < file_cfg.nattrs; ++i) {
        auto &attr = file_cfg.attributes[i];
        bzero(attr.ptr, attr.size);
    }

    auto err = tree_.find(id, &node_);
    if (err < 0) {
        file_ = found_file{};
        return err;
    }

    if (err == 0) {
        file_ = found_file{};
        return 0;
    }

    return 1;
}

found_file directory_tree::open() {
    assert(file_.id != UINT32_MAX);
    return file_;
}

} // namespace phylum
