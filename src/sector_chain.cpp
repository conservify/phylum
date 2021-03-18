#include "sector_chain.h"
#include "directory_chain.h" // For file_entry_t, etc...
#include "data_chain.h"

namespace phylum {

int32_t sector_chain::create_if_necessary() {
    logged_task lt{ "create" };

    if (head_ != InvalidSector || tail_ != InvalidSector) {
        return 0;
    }

    name("creating");

    auto err = grow_tail();
    if (err < 0) {
        return err;
    }

    debugf("%s: created, ready\n", name());

    return 0;
}

int32_t sector_chain::flush() {
    logged_task lt{ "sc-flush" };

    assert_valid();

    if (!dirty()) {
        debugf("%s flush (NOOP)\n", name());
        return 0;
    }

    debugf("%s flush\n", name());

    auto err = dhara_->write(sector_, buffer_.read_view().ptr(), buffer_.read_view().size());
    if (err < 0) {
        return err;
    }

    dirty(false);

    return 0;
}

int32_t sector_chain::seek_end_of_chain() {
    logged_task lt{ "seek-eoc" };

    assert_valid();

    debugf("%s starting\n", name());

    while (true) {
        auto err = forward();
        if (err < 0) {
            debugf("%s end (%d)\n", name(), err);
            return err;
        } else if (err == 0) {
            break;
        }
    }

    auto err = seek_end_of_buffer();
    if (err < 0) {
        return err;
    }

    debugf("%s sector=%d position=%zu\n", name(), sector_, db().position());

    return 0;
}

int32_t sector_chain::back_to_head() {
    if (sector_ != InvalidSector) {
        debugf("%s back-to-head %d -> %d\n", name(), sector_, head_);
        sector_ = InvalidSector;
    } else {
        debugf("%s back-to-head %d (NOOP)\n", name(), sector_);
    }

    length_ = 0;

    db().rewind();

    return 0;
}

int32_t sector_chain::back_to_tail() {
    assert(false);
    return 0;
}

int32_t sector_chain::forward() {
    logged_task lt{ "forward" };

    assert_valid();

    appendable(false);

    if (sector_ == InvalidSector) {
        debugf("%s first load\n", name());
        sector_ = head_;
    } else {
        auto hdr = header<sector_chain_header_t>();
        if (hdr->np == 0 || hdr->np == UINT32_MAX) {
            if (hdr->type == entry_type::DataSector) {
                auto dchdr = header<data_chain_header_t>();
                debugf("%s sector=%d bytes=%d length=%d (end)\n", name(), sector_, dchdr->bytes, length_);
            } else {
                debugf("%s sector=%d length=%d (end)\n", name(), sector_, length_);
            }
            return 0;
        }

        sector(hdr->np);

        if (hdr->type == entry_type::DataSector) {
            auto dchdr = header<data_chain_header_t>();
            debugf("%s sector=%d bytes=%d length=%d\n", name(), sector_, dchdr->bytes, length_);
        } else {
            debugf("%s sector=%d length=%d\n", name(), sector_, length_);
        }
    }

    db().clear();

    auto err = load();
    if (err < 0) {
        debugf("%s: load failed\n", name());
        return err;
    }

    length_++;

    return 1;
}

int32_t sector_chain::load() {
    logged_task lt{ "load" };

    assert_valid();

    if (sector_ == InvalidSector) {
        debugf("invalid sector\n");
        return -1;
    }

    db().rewind();

    return buffer_.unsafe_all([&](uint8_t *ptr, size_t size) {
        auto err = dhara_->read(sector_, ptr, size);
        if (err < 0) {
            return err;
        }
        return 0;
    });
}

int32_t sector_chain::log() {
    logged_task lt{ "log" };

    return walk([&](entry_t const *entry, written_record &record) {
        logged_task lt{ this->name() };

        switch (entry->type) {
        case entry_type::None: {
            debugf("none (%zu)\n", record.size_of_record);
            break;
        }
        case entry_type::SuperBlock: {
            auto sb = record.as<super_block_t>();
            debugf("super-block (%zu) version=%d\n", record.size_of_record, sb->version);
            break;
        }
        case entry_type::DirectorySector: {
            auto sh = record.as<sector_chain_header_t>();
            debugf("dir-sector (%zu) p=%d n=%d\n", record.size_of_record, sh->pp, sh->np);
            break;
        }
        case entry_type::DataSector: {
            auto sh = record.as<data_chain_header_t>();
            debugf("data-sector (%zu) p=%d n=%d bytes=%d\n", record.size_of_record, sh->pp, sh->np, sh->bytes);
            break;
        }
        case entry_type::FileEntry: {
            auto fe = record.as<file_entry_t>();
            debugf("entry (%zu) id=0x%x name='%s'\n", record.size_of_record, fe->id, fe->name);
            break;
        }
        case entry_type::FileData: {
            auto fd = record.as<file_data_t>();
            if (fd->size > 0) {
                debugf("data (%zu) id=0x%x size=%d\n", record.size_of_record, fd->id, fd->size);
            } else {
                debugf("data (%zu) id=0x%x chain=%d/%d\n", record.size_of_record, fd->id, fd->chain.head,
                       fd->chain.tail);

                data_chain dc{ *this, fd->chain };
                debugf("chain total-bytes=%d\n", dc.total_bytes());
            }
            break;
        }
        case entry_type::FileAttribute: {
            auto fa = record.as<file_attribute_t>();
            debugf("attr (%zu) id=0x%x attr=%d\n", record.size_of_record, fa->id, fa->type);
            break;
        }
        case entry_type::FileSkip: {
            auto fs = record.as<file_skip_t>();
            debugf("skip (%zu) id=0x%x sector=%d position=%d\n",
                   record.size_of_record, fs->id, fs->sector, fs->position);
            break;
        }
        }
        return 0;
    });
}

int32_t sector_chain::write_header_if_at_start() {
    if (db().position() > 0) {
        return 0;
    }

    debugf("%s write header\n", name());

    auto err = write_header();
    if (err < 0) {
        return err;
    }

    return 1;
}

int32_t sector_chain::grow_head() {
    assert(false);

    return 0;
}

int32_t sector_chain::grow_tail() {
    logged_task lt{ "grow" };

    auto previous_sector = sector_;
    auto allocated = allocator_->allocate();
    assert(allocated != sector_); // Don't ask.

    debugf("%s grow! %zu/%zu alloc=%d\n", name(), db().position(), db().size(), allocated);

    if (sector_ != InvalidSector) {
        assert(db().begin() != db().end());

        auto hdr = header<sector_chain_header_t>();
        assert(hdr != nullptr);
        hdr->np = allocated;
        dirty(true);

        auto err = flush();
        if (err < 0) {
            return err;
        }
    }

    tail(allocated);
    sector(allocated);

    buffer_.clear();

    auto err = write_header();
    if (err < 0) {
        return err;
    }

    auto hdr = header<sector_chain_header_t>();
    assert(hdr != nullptr);
    hdr->pp = previous_sector;

    return 0;
}

} // namespace phylum
