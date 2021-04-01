#pragma once

#include "phylum.h"
#include "entries.h"
#include "tree_sector.h"
#include "directory.h"
#include "sector_chain.h"

namespace phylum {

class directory_tree : public directory {
private:
    static constexpr size_t DataCapacity = 128 + 29;
    using value_type = dirtree_tree_value_t<DataCapacity>;
    tree_sector<uint32_t, value_type, 4> tree_;
    found_file file_;
    value_type node_;

public:
    directory_tree(working_buffers &buffers, sector_map &sectors, sector_allocator &allocator, dhara_sector_t root)
        : tree_(buffers, sectors, allocator, root, "dir-chain") {
    }

    virtual ~directory_tree() {
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

    int32_t read(file_id_t id, std::function<int32_t(simple_buffer&)> data_fn) override;

private:
    int32_t flush();

};

}
