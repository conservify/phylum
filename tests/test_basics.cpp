#include <directory_chain.h>
#include <file_appender.h>

#include "phylum_tests.h"
#include "geometry.h"

using namespace phylum;

class BasicsFixture : public PhylumFixture {
protected:
    FlashMemory memory{ sector_size() };

protected:
    size_t sector_size() const {
        return 256;
    }
};

TEST_F(BasicsFixture, EntrySizes) {
    EXPECT_EQ(sizeof(tree_node_t<uint32_t, uint32_t, 8>), 104u);
    EXPECT_EQ(sizeof(super_block_t), 9u);
    EXPECT_EQ(sizeof(directory_chain_header_t), 9u);
    EXPECT_EQ(sizeof(data_chain_header_t), 11u);
    EXPECT_EQ(sizeof(file_data_t), 29u);
    EXPECT_EQ(sizeof(file_attribute_t), 7u);
    EXPECT_EQ(sizeof(file_entry_t), 71u);
    EXPECT_EQ(sizeof(dirtree_entry_t), 67u);
    EXPECT_EQ(sizeof(dirtree_dir_t), 75u);
    EXPECT_EQ(sizeof(dirtree_file_t), 91u);
    EXPECT_EQ(sizeof(entry_t), 1u);
    EXPECT_EQ(sizeof(tree_node_header_t), 15u);
    // EXPECT_EQ(sizeof(dirtree_tree_value_t<1>), 128u);
    // EXPECT_EQ(sizeof(dirtree_tree_value_t<128>), 128u);
    // EXPECT_EQ(sizeof(tree_node_t<uint32_t, dirtree_tree_value_t<256 - 8>, 4>), 1024u);
}

TEST_F(BasicsFixture, MountFormatMount) {
    memory.begin(true);

    directory_chain chain{ memory.buffers(), memory.sectors(), memory.allocator(), 0 };
    ASSERT_EQ(chain.mount(), -1);
    ASSERT_EQ(chain.format(), 0);
    ASSERT_EQ(chain.flush(), 0);
    ASSERT_EQ(chain.mount(), 0);
}

TEST_F(BasicsFixture, FormatPersists) {
    memory.begin(true);

    memory.sync([&]() {
        directory_chain chain{ memory.buffers(), memory.sectors(), memory.allocator(), 0 };
        ASSERT_EQ(chain.format(), 0);
        ASSERT_EQ(chain.flush(), 0);
    });

    memory.sync([&]() {
        directory_chain chain{ memory.buffers(), memory.sectors(), memory.allocator(), 0 };
        ASSERT_EQ(chain.mount(), 0);
    });
}

TEST_F(BasicsFixture, FindAndTouch) {
    memory.mounted<directory_chain>([&](auto &chain) {
        ASSERT_EQ(chain.flush(), 0);
        ASSERT_EQ(chain.find("test.logs", open_file_config{}), 0);
        ASSERT_EQ(chain.touch("test.logs"), 0);
        ASSERT_EQ(chain.flush(), 0);
        ASSERT_EQ(chain.find("test.logs", open_file_config{}), 1);
    });
}

TEST_F(BasicsFixture, TouchPersists) {
    memory.mounted<directory_chain>([&](auto &chain) {
        ASSERT_EQ(chain.flush(), 0);
        ASSERT_EQ(chain.touch("test.logs"), 0);
        phydebugf("ok done");
    });

    memory.mounted<directory_chain>([&](auto &chain) {
        ASSERT_EQ(chain.find("test.logs", open_file_config{}), 0);
    });

    memory.mounted<directory_chain>([&](auto &chain) {
        ASSERT_EQ(chain.touch("test.logs"), 0);
        ASSERT_EQ(chain.flush(), 0);
    });

    memory.mounted<directory_chain>([&](auto &chain) {
        ASSERT_EQ(chain.find("test.logs", open_file_config{}), 1);
    });
}

TEST_F(BasicsFixture, TouchAndFindMultiple) {
    const char *names[] = {
        "a.txt",
        "b.txt",
        "c.txt",
        "d.txt",
        "e.txt",
        "f.txt",
        "g.txt"
    };
    memory.mounted<directory_chain>([&](auto &chain) {
        for (auto name : names) {
            ASSERT_EQ(chain.touch(name), 0);
        }
        ASSERT_EQ(chain.flush(), 0);
    });

    memory.mounted<directory_chain>([&](auto &chain) {
        ASSERT_EQ(chain.find("nope.txt", open_file_config{}), 0);
        for (auto name : names) {
            ASSERT_EQ(chain.find(name, open_file_config{}), 1);
        }
        ASSERT_EQ(chain.find("nope.txt", open_file_config{}), 0);

        sector_geometry sg{ memory.sectors() };
        EXPECT_TRUE(sg.sector(0).header<directory_chain_header_t>({ InvalidSector, 1 }));
        EXPECT_TRUE(sg.sector(0).nth<file_entry_t>(1, { names[0] }));
        EXPECT_TRUE(sg.sector(0).nth<file_entry_t>(2, { names[1] }));
        EXPECT_TRUE(sg.sector(0).nth<file_entry_t>(3, { names[2] }));
        EXPECT_TRUE(sg.sector(0).end(4));

        EXPECT_TRUE(sg.sector(1).header<directory_chain_header_t>({ 0, 2 }));
        EXPECT_TRUE(sg.sector(1).nth<file_entry_t>(1, { names[3] }));
        EXPECT_TRUE(sg.sector(1).nth<file_entry_t>(2, { names[4] }));
        EXPECT_TRUE(sg.sector(1).nth<file_entry_t>(3, { names[5] }));
        EXPECT_TRUE(sg.sector(1).end(4));

        EXPECT_TRUE(sg.sector(2).header<directory_chain_header_t>({ 1, InvalidSector }));
        EXPECT_TRUE(sg.sector(2).nth<file_entry_t>(1, { names[6] }));
        EXPECT_TRUE(sg.sector(2).end(2));
    });
}
