#pragma once

#include "phylum.h"
#include "entries.h"
#include "tree_sector.h"
#include "directory.h"

namespace phylum {

class directory_tree : public directory {
private:
    using value_type = dirtree_tree_value_t<128 + 29>;
    tree_sector<uint32_t, value_type, 4> tree_;
    found_file file_;
    value_type node_;

public:
    directory_tree(working_buffers &buffers, sector_map &sectors, sector_allocator &allocator, dhara_sector_t root, const char *prefix)
        : tree_(buffers, sectors, allocator, root, prefix) {
    }

    virtual ~directory_tree() {
    }

public:
    int32_t mount();

    int32_t format();

    int32_t touch(const char *name);

    int32_t find(const char *name, open_file_config file_cfg);

    found_file open();

    friend class dirtree_file_appender;

    friend class dirtree_file_reader;

protected:
    int32_t file_attributes(file_id_t id, open_file_attribute *attributes, size_t nattrs) override;

    int32_t file_chain(file_id_t id, head_tail_t chain) override;

    int32_t file_data(file_id_t id, uint8_t const *buffer, size_t size) override;

    int32_t read(file_id_t id, std::function<int32_t(simple_buffer&)> fn) override;

};

}
