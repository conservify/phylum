#pragma once

#include "delimited_buffer.h"
#include "dhara_map.h"
#include "sector_allocator.h"

namespace phylum {

using default_node_type = tree_node_t<uint32_t, uint32_t, 6, 6>;

struct persisted_node_t {
    default_node_type *node{ nullptr };
    node_ptr_t ptr{};
};

struct insertion_t {
    bool split{ false };
    uint32_t key{ 0 };
    node_ptr_t left;
    node_ptr_t right;
};

class tree_sector {
private:
    static constexpr size_t ScopeNameLength = 32;

private:
    dhara_map *dhara_;
    sector_allocator *allocator_;
    delimited_buffer buffer_;
    dhara_sector_t sector_{ InvalidSector };
    dhara_sector_t root_{ InvalidSector };
    bool dirty_{ false };
    const char *prefix_{ "tree-sector" };
    char name_[ScopeNameLength];

public:
    tree_sector(dhara_map &dhara, sector_allocator &allocator, simple_buffer &&buffer, dhara_sector_t root,
                const char *prefix)
        : dhara_(&dhara), allocator_(&allocator), buffer_(std::move(buffer)), root_(root), prefix_(prefix) {
        name("%s[%d]", prefix_, root_);
    }

    tree_sector(tree_sector &other, dhara_sector_t root, const char *prefix)
        : dhara_(other.dhara_), allocator_(other.allocator_), buffer_(other.sector_size()), root_(root),
          prefix_(prefix) {
        name("%s[%d]", prefix_, root_);
    }

    virtual ~tree_sector() {
    }

public:
    int32_t add(uint32_t key, uint32_t value);
    int32_t find(uint32_t key, uint32_t &value);

public:
    const char *name() const {
        return name_;
    }

    dhara_sector_t sector() const {
        return sector_;
    }

    void dirty(bool value) {
        dirty_ = value;
        if (value) {
            phydebugf("%s dirty", name());
        }
    }

    bool dirty() {
        return dirty_;
    }

    int32_t create();

    int32_t back_to_root();

    int32_t log();

protected:
    void name(const char *f, ...);

    size_t sector_size() const {
        return buffer_.size();
    }

    delimited_buffer &db() {
        return buffer_;
    }

    sector_allocator &allocator() {
        return *allocator_;
    }

    // Allow users to call this to set the sector and defer the read
    // to when the code actually needs to do a read.
    void sector(dhara_sector_t sector) {
        sector_ = sector;
        name("%s[%d]", prefix_, sector_);
    }

private:
    int32_t log(default_node_type *node);

    int32_t leaf_insert_nonfull(default_node_type *node, uint32_t &key, uint32_t &value, unsigned index);

    int32_t inner_insert_nonfull(depth_type current_depth, default_node_type *node, node_ptr_t node_ptr, uint32_t key,
                                 uint32_t value);

    int32_t inner_node_insert(depth_type current_depth, default_node_type *node, node_ptr_t node_ptr, uint32_t key,
                              uint32_t value, insertion_t &insertion);

    int32_t leaf_node_insert(default_node_type *node, node_ptr_t node_ptr, uint32_t key, uint32_t value,
                             insertion_t &insertion);

    int32_t allocate_node(default_node_type *node, node_ptr_t &ptr);

    int32_t follow_node_ptr(node_ptr_t &ptr, persisted_node_t &followed);

    int32_t flush();
};

} // namespace phylum
