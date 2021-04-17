#include "string_format.h"

#include <directory_chain.h>
#include <directory_tree.h>
#include <file_appender.h>
#include <file_reader.h>
#include <super_chain.h>

#include "phylum_tests.h"
#include "geometry.h"

using namespace phylum;

template <typename T> class IndexedFixture : public PhylumFixture {};

template<typename TLayout, typename TDirectory, typename TTree>
struct test_types {
    using layout_type = TLayout;
    using directory_type = TDirectory;
    using tree_type = TTree;
};

typedef ::testing::Types<
    test_types<layout_4096, directory_tree, tree_sector<uint32_t, uint32_t, 63>>,
    test_types<layout_4096, directory_tree, tree_sector<uint32_t, uint32_t, 405>>>
    Implementations;

TYPED_TEST_SUITE(IndexedFixture, Implementations);

TYPED_TEST(IndexedFixture, TouchedIndexed) {
    using layout_type = typename TypeParam::layout_type;
    using directory_type = typename TypeParam::directory_type;
    using tree_type = typename TypeParam::tree_type;

    layout_type layout;
    FlashMemory memory{ layout.sector_size };

    memory.mounted<super_chain>([&](super_chain &super) {
        directory_type dir{ memory.pc(), memory.allocator().allocate() };
        ASSERT_EQ(dir.format(), 0);
        ASSERT_EQ(dir.template touch_indexed<tree_type>("data.txt"), 0);
    });
}

TYPED_TEST(IndexedFixture, TouchedIndexed_10) {
    using layout_type = typename TypeParam::layout_type;
    using directory_type = typename TypeParam::directory_type;
    using tree_type = typename TypeParam::tree_type;

    layout_type layout;
    FlashMemory memory{ layout.sector_size };

    std::vector<std::string> files;

    memory.mounted<super_chain>([&](super_chain &super) {
        directory_type dir{ memory.pc(), memory.allocator().allocate() };
        ASSERT_EQ(dir.format(), 0);
        ASSERT_EQ(super.update(dir.to_tree_ptr()), 0);
    });

    for (auto i = 0; i < 10; ++i) {
        memory.mounted<super_chain>([&](super_chain &super) {
            directory_type dir{ memory.pc(), super.directory_tree() };

            std::string name = string_format("data-%d.txt", i);
            ASSERT_EQ(dir.template touch_indexed<tree_type>(name.c_str()), 0);
            files.push_back(name);

            ASSERT_EQ(super.update(dir.to_tree_ptr()), 0);
        });
    }

    memory.mounted<super_chain>([&](super_chain &super) {
        directory_type dir{ memory.pc(), super.directory_tree() };

        for (auto name : files) {
            ASSERT_EQ(dir.template find(name.c_str(), open_file_config{ }), 1);
        }
    });
}

TYPED_TEST(IndexedFixture, DISABLED_TouchedIndexed_100_FindAfterEach) {
    using layout_type = typename TypeParam::layout_type;
    using directory_type = typename TypeParam::directory_type;
    using tree_type = typename TypeParam::tree_type;

    layout_type layout;
    FlashMemory memory{ layout.sector_size };

    memory.mounted<super_chain>([&](super_chain &super) {
        directory_type dir{ memory.pc(), memory.allocator().allocate() };
        ASSERT_EQ(dir.format(), 0);
        ASSERT_EQ(super.update(dir.to_tree_ptr()), 0);
    });

    std::vector<std::string> files;

    for (auto i = 0; i < 100; ++i) {
        std::string name = string_format("data-%d.txt", i);

        auto pass = true;

        memory.mounted<super_chain>([&](super_chain &super) {
            directory_type dir{ memory.pc(), super.directory_tree() };
            // ASSERT_EQ(dir.template touch_indexed<tree_type>(name.c_str()), 0);
            ASSERT_EQ(dir.touch(name.c_str()), 0);
            ASSERT_EQ(dir.find(name.c_str(), open_file_config{ }), 1);
            ASSERT_EQ(super.update(dir.to_tree_ptr()), 0);
        });

        memory.mounted<super_chain>([&](super_chain &super) {
            directory_type dir{ memory.pc(), super.directory_tree() };
            auto err = dir.find(name.c_str(), open_file_config{ });
            if (err != 1) {
                pass = false;
            }
            ASSERT_EQ(err, 1);
        });

        ASSERT_TRUE(pass);

        files.push_back(name);
    }
}

TYPED_TEST(IndexedFixture, DISABLED_TouchedIndexed_100) {
    using layout_type = typename TypeParam::layout_type;
    using directory_type = typename TypeParam::directory_type;
    using tree_type = typename TypeParam::tree_type;

    layout_type layout;
    FlashMemory memory{ layout.sector_size };

    memory.mounted<super_chain>([&](super_chain &super) {
        directory_type dir{ memory.pc(), memory.allocator().allocate() };
        ASSERT_EQ(dir.format(), 0);
        ASSERT_EQ(super.update(dir.to_tree_ptr()), 0);
    });

    std::vector<std::string> files;

    for (auto i = 0; i < 100; ++i) {
        std::string name = string_format("data-%d.txt", i);
        memory.mounted<super_chain>([&](super_chain &super) {
            directory_type dir{ memory.pc(), super.directory_tree() };
            ASSERT_EQ(dir.template touch_indexed<tree_type>(name.c_str()), 0);
            ASSERT_EQ(super.update(dir.to_tree_ptr()), 0);
        });
        files.push_back(name);
    }

    memory.mounted<super_chain>([&](super_chain &super) {
        directory_type dir{ memory.pc(), super.directory_tree() };
        for (auto name : files) {
            ASSERT_EQ(dir.find(name.c_str(), open_file_config{ }), 1);
        }
    });
}
