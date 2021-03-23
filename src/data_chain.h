#pragma once

#include "sector_chain.h"

namespace phylum {

enum seek_reference {
    Start,
    End,
};

class data_chain : public sector_chain {
private:
    head_tail_t chain_;

public:
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

protected:
    int32_t write_header() override;

    int32_t seek_end_of_buffer() override;

    template <typename T>
    int32_t write_chain(T data_fn) {
        logged_task lt{ "write-data-chain" };

        if (!appendable()) {
            debugf("making appendable\n");

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
                auto hdr = header<data_chain_header_t>();
                debugf("write resuming sector-bytes=%d\n", hdr->bytes);
                assert(db().skip(hdr->bytes) >= 0);
            }

            appendable(true);
        }

        auto written = 0;

        while (true) {
            debugf("view position=%zu available=%zu size=%zu\n", db().position(), db().available(), db().size());

            auto grow = false;
            auto rv = data_fn(db().write_view(), grow);
            if (rv < 0) {
                return rv;
            }

            // Do this before we grow so the details are saved.
            auto hdr = header<data_chain_header_t>();
            assert(hdr->bytes + rv < (int32_t)sector_size());
            hdr->bytes += rv;
            written += rv;

            dirty(true);

            // Grow and write header.
            if (grow) {
                auto err = grow_tail();
                if (err < 0) {
                    return err;
                }
            } else {
                if (rv == 0) {
                    break;
                }
            }
        }
        return written;
    }

    template <typename T>
    int32_t read_chain(T data_fn) {
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
                auto err = db().seek_once();
                if (err < 0) {
                    return err;
                }

                // Constrain is relative, by the way so this will preventing
                // reading from more than hdr->bytes.
                auto hdr = header<data_chain_header_t>();
                assert(db().constrain(hdr->bytes) >= 0);

                debugf("read resuming sector-bytes=%d\n", hdr->bytes);
            }

            // If we have data available.
            if (db().available() > 0) {
                debugf("view position=%zu available=%zu\n", db().position(), db().available());

                auto rv = data_fn(db().read_view());
                if (rv < 0) {
                    return rv;
                }
                if (rv >= 0) {
                    return rv;
                }
            }

            auto err = forward();
            if (err < 0) {
                return err;
            } else if (err == 0) {
                break;
            }
        }

        return 0;
    }
};

} // namespace phylum
