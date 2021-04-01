#include "file_appender.h"

namespace phylum {

file_appender::file_appender(directory_chain &directory, found_file file, simple_buffer &&buffer)
    : directory_(directory), file_(file), buffer_(std::move(buffer)), data_chain_(directory, file.chain) {
}

file_appender::~file_appender() {
}

int32_t file_appender::write(uint8_t const *data, size_t size) {
    logged_task lt{ "fa-write" };

    return buffer_.fill(data, size, [&](simple_buffer &) {
        return flush();
    });
}

int32_t file_appender::make_data_chain() {
    logged_task lt{ "fa-mdc" };

    auto err = data_chain_.create_if_necessary();
    if (err < 0) {
        return err;
    }

    phydebugf("%s finding inline data begin", directory_.name());

    err = directory_.read(file_.id, [&](simple_buffer &data_buffer) {
        auto err = data_chain_.write(data_buffer.ptr(), data_buffer.size());
        if (err < 0) {
            return err;
        }
        return 0;
    });
    if (err < 0) {
        return err;
    }

    if (buffer_.position() > 0) {
        auto err = data_chain_.write(buffer_.ptr(), buffer_.position());
        if (err < 0) {
            return err;
        }

        buffer_.clear();
    }

    phydebugf("%s finding inline data end", directory_.name());

    return 0;
}

int32_t file_appender::flush() {
    logged_task lt{ "fa-flush" };

    assert(file_.id != UINT32_MAX);

    // Do we already have a data chain?
    auto had_chain = data_chain_.valid();
    if (had_chain) {
        phyinfof("writing to chain");

        auto err = data_chain_.write(buffer_.ptr(), buffer_.position());
        if (err < 0) {
            return err;
        }

        buffer_.clear();
    } else {
        auto pending = buffer_.position();
        if (pending == 0) {
            return 0;
        }

        if (pending < buffer_.size() / 2) {
            phyinfof("flush: inline id=0x%x bytes=%zu begin", file_.id, pending);

            assert(directory_.file_data(file_.id, buffer_.ptr(), buffer_.position()) >= 0);

            buffer_.clear();

            phyinfof("flush: inline done");
        } else {
            phyinfof("flush making chain (%d)", buffer_.position());

            auto err = make_data_chain();
            if (err < 0) {
                return err;
            }

            phyinfof("flush making chain done (%d)", buffer_.position());
        }
    }

    if (data_chain_.valid()) {
        phydebugf("flush remaining (%d)", buffer_.position());

        auto err = data_chain_.flush();
        if (err < 0) {
            return err;
        }

        if (!had_chain) {
            phyinfof("%s updating directory", data_chain_.name());
            file_.chain.head = data_chain_.head();
            file_.chain.tail = data_chain_.tail();
            err = directory_.file_chain(file_.id, file_.chain);
            if (err < 0) {
                return err;
            }

            return 0;
        }
    }

    return 0;
}

uint32_t file_appender::u32(uint8_t type) {
    assert(file_.cfg.nattrs > 0);
    for (auto i = 0u; i < file_.cfg.nattrs; ++i) {
        auto &attr = file_.cfg.attributes[i];
        if (attr.type == type) {
            assert(sizeof(uint32_t) == attr.size);
            return *(uint32_t *)attr.ptr;
        }
    }
    assert(false);
    return 0;
}

void file_appender::u32(uint8_t type, uint32_t value) {
    assert(file_.cfg.nattrs > 0);
    for (auto i = 0u; i < file_.cfg.nattrs; ++i) {
        auto &attr = file_.cfg.attributes[i];
        if (attr.type == type) {
            assert(sizeof(uint32_t) == attr.size);
            if (*(uint32_t *)attr.ptr != value) {
                *(uint32_t *)attr.ptr = value;
                attr.dirty = true;
            }
            return;
        }
    }
    assert(false);
}

int32_t file_appender::close() {
    logged_task lt{ "fa-close" };

    auto err = flush();
    if (err < 0) {
        return err;
    }

    err = directory_.file_attributes(file_.id, file_.cfg.attributes, file_.cfg.nattrs);
    if (err < 0) {
        return err;
    }

    for (auto i = 0u; i < file_.cfg.nattrs; ++i) {
        auto &attr = file_.cfg.attributes[i];
        attr.dirty = false;
    }

    return 0;
}

} // namespace phylum
