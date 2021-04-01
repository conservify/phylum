#pragma once

#include <cinttypes>
#include <cstddef>

namespace phylum {

constexpr size_t MaximumNameLength = 64;

typedef uint32_t file_id_t;
typedef uint32_t file_size_t;
typedef uint16_t file_flags_t;
typedef uint16_t record_number_t;
typedef uint16_t sector_offset_t;
typedef uint32_t dhara_sector_t;

constexpr dhara_sector_t InvalidSector = (dhara_sector_t)-1;

#define PHY_PACKED __attribute__((__packed__))

struct PHY_PACKED head_tail_t {
    dhara_sector_t head{ InvalidSector };
    dhara_sector_t tail{ InvalidSector };

    head_tail_t() {
    }

    head_tail_t(dhara_sector_t head, dhara_sector_t tail) : head(head), tail(tail) {
    }

    bool valid() const {
        return head != InvalidSector;
    }
};

enum entry_type : uint8_t {
    None = 0,
    SuperBlock = 1,
    DataSector = 2,
    DirectorySector = 3,
    FileEntry = 4,
    FsFileEntry = 9,
    FsDirectoryEntry = 5,
    FileData = 6,
    TreeNode = 7,
    FileAttribute = 8,
};

struct PHY_PACKED entry_t {
    entry_type type{ entry_type::None };

    entry_t(entry_type type) : type(type) {
    }
};

struct PHY_PACKED sector_chain_header_t : entry_t {
    dhara_sector_t pp{ (dhara_sector_t)InvalidSector };
    dhara_sector_t np{ (dhara_sector_t)InvalidSector };

    sector_chain_header_t(entry_type type) : entry_t(type) {
    }

    sector_chain_header_t(entry_type type, dhara_sector_t pp, dhara_sector_t np) : entry_t(type), pp(pp), np(np) {
    }
};

struct PHY_PACKED directory_chain_header_t : sector_chain_header_t {
    directory_chain_header_t() : sector_chain_header_t(entry_type::DirectorySector) {
    }

    directory_chain_header_t(dhara_sector_t pp, dhara_sector_t np) : sector_chain_header_t(entry_type::DirectorySector, pp, np) {
    }
};

struct PHY_PACKED data_chain_header_t : sector_chain_header_t {
    uint16_t bytes{ 0 };

    data_chain_header_t() : sector_chain_header_t(entry_type::DataSector) {
    }

    data_chain_header_t(uint16_t bytes) : sector_chain_header_t(entry_type::DataSector), bytes(bytes) {
    }

    data_chain_header_t(uint16_t bytes, dhara_sector_t pp, dhara_sector_t np) : sector_chain_header_t(entry_type::DataSector, pp, np), bytes(bytes) {
    }
};

struct PHY_PACKED super_block_t : entry_t {
    uint32_t version{ 0 };
    uint32_t reserved{ (uint32_t)-1 };

    super_block_t(uint32_t version) : entry_t(entry_type::SuperBlock), version(version) {
    }
};

inline uint32_t make_file_id(const char *path) {
    return crc32_checksum(path);
}

struct PHY_PACKED dirtree_entry_t : entry_t {
    char name[MaximumNameLength];
    file_flags_t flags;

    dirtree_entry_t(entry_type type, const char *full_name, uint16_t flags) : entry_t(type), flags(flags) {
        bzero(name, sizeof(name));
        strncpy(name, full_name, sizeof(name));
    }
};

struct PHY_PACKED dirtree_dir_t : dirtree_entry_t {
    dhara_sector_t attributes{ InvalidSector };
    dhara_sector_t children{ InvalidSector };

    dirtree_dir_t(const char *name, uint16_t flags = 0)
        : dirtree_entry_t(entry_type::FsDirectoryEntry, name, flags) {
    }
};

struct PHY_PACKED dirtree_file_t : dirtree_entry_t {
    file_size_t directory_size{ 0 };
    head_tail_t chain;
    dhara_sector_t attributes{ InvalidSector };
    dhara_sector_t position_index{ InvalidSector };
    dhara_sector_t record_index{ InvalidSector };

    dirtree_file_t(const char *name, uint16_t flags = 0)
        : dirtree_entry_t(entry_type::FsFileEntry, name, flags) {
    }
};

template<size_t Storage>
struct PHY_PACKED dirtree_tree_value_t {
    union PHY_PACKED entry_union {
        dirtree_entry_t e;
        dirtree_dir_t dir;
        dirtree_file_t file;

        entry_union() {
        }
    } u;

    uint8_t data[Storage];

    dirtree_tree_value_t() {
    }
};

struct PHY_PACKED file_entry_t : entry_t {
    file_id_t id;
    file_flags_t flags;
    char name[MaximumNameLength];

    file_entry_t(file_id_t id, const char *full_name, uint16_t flags = 0)
        : entry_t(entry_type::FileEntry), id(id), flags(flags) {
        bzero(name, sizeof(name));
        strncpy(name, full_name, sizeof(name));
    }

    file_entry_t(const char *full_name, uint16_t flags = 0)
        : entry_t(entry_type::FileEntry), id(make_file_id(full_name)), flags(flags) {
        bzero(name, sizeof(name));
        strncpy(name, full_name, sizeof(name));
    }
};

struct PHY_PACKED file_data_t : entry_t {
    file_id_t id{ 0 };
    file_size_t size{ 0 };
    head_tail_t chain;
    dhara_sector_t attributes{ InvalidSector };
    dhara_sector_t position_index{ InvalidSector };
    dhara_sector_t record_index{ InvalidSector };

    file_data_t(file_id_t id, file_size_t size) : entry_t(entry_type::FileData), id(id), size(size) {
    }

    file_data_t(file_id_t id, head_tail_t chain)
        : entry_t(entry_type::FileData), id(id), chain(chain) {
    }
};

struct PHY_PACKED file_attribute_t : entry_t {
    file_id_t id{ 0 };
    uint8_t type{ 0 };
    uint8_t size{ 0 };

    file_attribute_t(file_id_t id, uint8_t type, uint8_t size)
        : entry_t(entry_type::FileAttribute), id(id), type(type), size(size) {
    }
};

struct PHY_PACKED node_ptr_t {
    dhara_sector_t sector;
    sector_offset_t position;

    node_ptr_t() : sector(InvalidSector), position(0) {
    }

    node_ptr_t(dhara_sector_t sector, sector_offset_t position): sector(sector), position(position) {
    }
};

using depth_type = uint8_t;
using index_type = uint16_t;

enum node_type : uint8_t {
    Leaf,
    Inner,
};

struct PHY_PACKED tree_node_header_t : entry_t {
public:
    file_id_t id{ 0 };
    depth_type depth{ 0 };
    index_type number_keys{ 0 };
    node_type type{ node_type::Leaf };
    node_ptr_t parent;

public:
    tree_node_header_t() : entry_t(entry_type::TreeNode), parent() {
    }

    tree_node_header_t(node_type type) : entry_t(entry_type::TreeNode), type(type), parent() {
    }
};

template <typename KEY, typename VALUE, size_t Size>
struct tree_node_t : tree_node_header_t {
public:
    typedef KEY key_type;
    typedef VALUE value_type;

    union data_t {
        VALUE values[Size];
        node_ptr_t children[Size + 1];

        data_t() {
        }
    };

public:
    KEY keys[Size];
    data_t d;

public:
    tree_node_t() : tree_node_header_t() {
        clear();
    }

    tree_node_t(node_type type) : tree_node_header_t(type) {
        clear();
    }

public:
    void clear() {
        depth = 0;
        number_keys = 0;
        for (auto i = 0u; i < Size; ++i) {
            keys[i] = 0;
            d.values[i] = {};
            d.children[i] = {};
        }
        for (auto &ref : d.children) {
            ref = {};
        }
    }

    bool empty() const {
        return number_keys == 0;
    }
};

} // namespace phylum
