#pragma once

#include "phylum.h"
#include "sector_chain.h"

namespace phylum {

class file_appender;

struct open_file_attribute {
    uint8_t type{ 0 };
    uint8_t size{ 0 };
    void   *ptr{ nullptr };
    bool    dirty{ 0 };
};

struct open_file_config {
    open_file_attribute *attributes{ nullptr };
    size_t nattrs{ 0 };
};

struct found_file {
    file_id_t id{ UINT32_MAX };
    file_size_t size{ UINT32_MAX };
    head_tail_t chain;
    open_file_config cfg;
};

class directory_chain : public sector_chain {
private:
    found_file file_;

public:
    directory_chain(working_buffers &buffers, sector_map &sectors, sector_allocator &allocator, dhara_sector_t head)
        : sector_chain(buffers, sectors, allocator, head_tail_t{ head, InvalidSector }, "directory") {
    }

    directory_chain(sector_chain &other, dhara_sector_t head)
        : sector_chain(other, { head, InvalidSector }, "directory") {
    }

    virtual ~directory_chain() {
    }

public:
    found_file file() const {
        return file_;
    }

public:
    int32_t mount();

    int32_t format();

    int32_t touch(const char *name);

    int32_t find(const char *name, open_file_config file_cfg);

    found_file open();

    template<typename T>
    int32_t read(file_id_t id, T data_fn) {
        return walk([&](entry_t const *entry, written_record &record) {
            if (entry->type == entry_type::FileData) {
                auto fd = record.as<file_data_t>();
                if (fd->id == id) {
                    phydebugf("%s (copy) id=0x%x bytes=%d size=%d", name(), fd->id, fd->size, file_.size);

                    auto data_buffer = record.data<file_data_t>();
                    auto err = data_fn(data_buffer);
                    if (err < 0) {
                        return err;
                    }
                }
            }
            return (int32_t)0;
        });
    }

    friend class file_appender;

    friend class file_reader;

protected:
    int32_t write_header() override;

    int32_t seek_end_of_buffer() override;

    int32_t file_attribute(file_id_t id, open_file_attribute attribute);

    int32_t file_attributes(file_id_t id, open_file_attribute *attributes, size_t nattrs);

    int32_t file_chain(file_id_t id, head_tail_t chain);

    int32_t file_data(file_id_t id, uint8_t const *buffer, size_t size);

    int32_t read(file_id_t id, uint8_t *buffer, size_t size);

    int32_t seek_file_entry(file_id_t id);

protected:
    template <typename T, class... Args>
    int32_t emplace(Args &&... args) {
        assert(sizeof(T) <= db().size());

        auto err = prepare(sizeof(T));
        if (err < 0) {
            return err;
        }

        logged_task lt{ name() };

        db().emplace<T, Args...>(std::forward<Args>(args)...);

        dirty(true);

        return 0;
    }

    template <typename T>
    int32_t append(T &record, uint8_t const *buffer, size_t size) {
        assert(sizeof(T) + size <= db().size());

        auto err = prepare(sizeof(T) + size);
        if (err < 0) {
            return err;
        }

        logged_task lt{ name() };

        db().append<T>(record, buffer, size);

        dirty(true);

        return 0;
    }

private:
    int32_t grow_if_necessary(size_t required);

    int32_t prepare(size_t required);
};

} // namespace phylum
