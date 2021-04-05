#pragma once

#include "phylum.h"
#include "sector_chain.h"
#include "directory.h"

namespace phylum {

class directory_chain : public sector_chain, public directory {
private:
    found_file file_;

public:
    directory_chain(working_buffers &buffers, sector_map &sectors, sector_allocator &allocator, dhara_sector_t head)
        : sector_chain(buffers, sectors, allocator, head_tail_t{ head, InvalidSector }, "dir-chain") {
    }

    directory_chain(sector_chain &other, dhara_sector_t head)
        : sector_chain(other, { head, InvalidSector }, "dir-chain") {
    }

    virtual ~directory_chain() {
    }

public:
    friend class file_appender;

    friend class file_reader;

    int32_t mount() override;

    int32_t format() override;

    int32_t touch(const char *name) override;

    int32_t find(const char *name, open_file_config file_cfg) override;

    found_file open() override;

protected:
    int32_t file_data(file_id_t id, uint8_t const *buffer, size_t size) override;

    int32_t file_chain(file_id_t id, head_tail_t chain) override;

    int32_t file_attributes(file_id_t id, open_file_attribute *attributes, size_t nattrs) override;

    int32_t read(file_id_t id, std::function<int32_t(read_buffer)> data_fn) override;

private:
    int32_t write_header(page_lock &page_lock) override;

    int32_t seek_end_of_buffer(page_lock &page_lock) override;

    int32_t seek_file_entry(file_id_t id);

    int32_t file_attribute(file_id_t id, open_file_attribute attribute);

    int32_t prepare(page_lock &page_lock, size_t required);

    int32_t grow_if_necessary(page_lock &page_lock, size_t required);

    template <typename T, class... Args>
    int32_t emplace(page_lock &page_lock, Args &&... args) {
        assert(sizeof(T) <= db().size());

        auto err = prepare(page_lock, sizeof(T));
        if (err < 0) {
            return err;
        }

        logged_task lt{ name() };

        db().emplace<T, Args...>(std::forward<Args>(args)...);

        dirty(true);

        return 0;
    }

    template <typename T>
    int32_t append(page_lock &page_lock, T &record, uint8_t const *buffer, size_t size) {
        assert(sizeof(T) + size <= db().size());

        auto err = prepare(page_lock, sizeof(T) + size);
        if (err < 0) {
            return err;
        }

        logged_task lt{ name() };

        db().append<T>(record, buffer, size);

        dirty(true);

        return 0;
    }

};

} // namespace phylum
