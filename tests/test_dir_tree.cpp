#include <directory_tree.h>

#include "phylum_tests.h"
#include "geometry.h"

using namespace phylum;

template<typename T>
class DirectoryTreeFixture : public ::testing::Test {
};

typedef ::testing::Types<std::pair<layout_4096, layout_4096>> Implementations;

TYPED_TEST_SUITE(DirectoryTreeFixture, Implementations);

TYPED_TEST(DirectoryTreeFixture, CreatingAndTouchFiles) {
    typename TypeParam::first_type layout;
    FlashMemory memory{ layout.sector_size };
    memory.mounted<directory_chain>([&](auto &chain) {
        auto first = memory.allocator().allocate();

        directory_tree dir(memory.buffers(), memory.sectors(), memory.allocator(), first, "tree");
        if (dir.mount() < 0) {
            ASSERT_EQ(dir.format(), 0);
        }

        ASSERT_EQ(dir.find("other.logs", open_file_config{}), 0);
        ASSERT_EQ(dir.find("test.logs", open_file_config{}), 0);
        ASSERT_EQ(dir.touch("test.logs"), 0);
        ASSERT_EQ(dir.find("test.logs", open_file_config{}), 1);
        ASSERT_EQ(dir.find("other.logs", open_file_config{}), 0);
    });
}
