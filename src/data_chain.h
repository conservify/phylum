#pragma once

#include "sector_chain.h"

namespace phylum {

enum seek_reference {
    Start,
    End,
};

struct data_chain_cursor {
    dhara_sector_t sector{ 0 };
    file_size_t position{ 0 };
    file_size_t position_at_start_of_sector{ 0 };

    data_chain_cursor() {
    }

    data_chain_cursor(dhara_sector_t sector) : sector(sector), position(0), position_at_start_of_sector(0) {
    }

    data_chain_cursor(dhara_sector_t sector, file_size_t position, file_size_t position_at_start_of_sector) : sector(sector), position(position), position_at_start_of_sector(position_at_start_of_sector) {
    }
};

class data_chain : public sector_chain {
private:
    head_tail_t chain_{ };
    file_size_t position_{ 0 };
    file_size_t position_at_start_of_sector_{ 0 };

public:
    data_chain(working_buffers &buffers, sector_map &sectors, sector_allocator &allocator, head_tail_t chain, const char *prefix)
        : sector_chain(buffers, sectors, allocator, chain, prefix) {
    }

    data_chain(sector_chain &other, head_tail_t chain) : sector_chain(other, chain, "data"), chain_(chain) {
    }

    data_chain(sector_chain &other, head_tail_t chain, const char *prefix)
        : sector_chain(other, chain, prefix), chain_(chain) {
    }

    virtual ~data_chain() {
    }

public:
    int32_t write(uint8_t const *data, size_t size);
    int32_t read(uint8_t *data, size_t size);
    uint32_t total_bytes();
    int32_t seek(file_size_t position, seek_reference reference);

public:
    data_chain_cursor cursor() {
        return data_chain_cursor{ sector(), position_, position_at_start_of_sector_ };
    }

protected:
    int32_t write_header() override;

    int32_t seek_end_of_buffer() override;

    int32_t write_chain(std::function<int32_t(write_buffer, bool&)> data_fn) {
        logged_task lt{ "write-data-chain" };

        if (!appendable()) {
            phydebugf("making appendable");

            assert(back_to_head() >= 0);

            logged_task lt{ name() };

            auto err = seek_end_of_chain();
            if (err < 0) {
                return err;
            }

            err = write_header_if_at_start();
            if (err < 0) {
                return err;
            }

            if (err == 0) {
                auto hdr = db().header<data_chain_header_t>();
                phydebugf("write resuming sector-bytes=%d", hdr->bytes);
                assert(db().skip(hdr->bytes) >= 0);
            }

            appendable(true);
        }

        auto written = 0;

        while (true) {
            phydebugf("write: position=%zu available=%zu size=%zu", db().position(), db().available(), db().size());

            auto grow = false;
            auto err = db().write_view([&](write_buffer wb) {
                auto err = data_fn(std::move(wb), grow);
                if (err < 0) {
                    return err;
                }

                // Do this before we grow so the details are saved.
                auto hdr = db().header<data_chain_header_t>();
                assert(hdr->bytes + err <= (int32_t)sector_size());
                hdr->bytes += err;
                written += err;
                position_ += err;
                db().skip(err); // TODO Remove

                return err;
            });
            if (err < 0) {
                return err;
            }

            dirty(true);

            // Grow and write header.
            if (grow) {
                auto err = grow_tail();
                if (err < 0) {
                    return err;
                }

                position_at_start_of_sector_ = position_;
            } else {
                if (err == 0) {
                    break;
                }
            }
        }
        return written;
    }

    int32_t read_chain(std::function<int32_t(read_buffer)> data_fn) {
        logged_task lt{ "read-data-chain", name() };

        assert_valid();

        auto err = ensure_loaded();
        if (err < 0) {
            return err;
        }

        while (true) {
            // If we're at the start of the buffer, seek past the
            // data_chain_header so we can read the number of bytes in
            // this sector.
            if (db().position() == 0) {
                auto err = db().seek_end();
                if (err < 0) {
                    return err;
                }

                db().skip(1); // HACK TODO Skip terminator.

                // Constrain is relative, by the way so this will preventing
                // reading from more than hdr->bytes.
                auto hdr = db().header<data_chain_header_t>();
                assert(db().constrain(hdr->bytes) >= 0);

                phydebugf("read resuming sector-bytes=%d position=%d", hdr->bytes, db().position());
            }

            // If we have data available.
            if (db().available() > 0) {
                phydebugf("view position=%zu available=%zu", db().position(), db().available());

                auto err = data_fn(db().to_read_buffer());
                if (err < 0) {
                    return err;
                }
                if (err >= 0) {
                    position_ += err;
                    db().skip(err); // TODO Remove
                    return err;
                }
            }

            auto err = forward();
            if (err < 0) {
                return err;
            } else if (err == 0) {
                break;
            }

            position_at_start_of_sector_ = position_;
        }

        return 0;
    }
};

} // namespace phylum
