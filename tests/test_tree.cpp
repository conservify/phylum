#include <directory_chain.h>
#include <file_appender.h>
#include <tree_sector.h>

#include "suite_base.h"
#include "geometry.h"

using namespace phylum;

class TreeSuite : public PhylumSuite {};

TEST_F(TreeSuite, SingleNodeTree) {
    mounted([&](directory_chain &chain) {
        auto first = allocator().allocate();
        tree_sector tree{ dhara(), allocator(), simple_buffer{ sector_size() }, first, "tree" };

        ASSERT_EQ(tree.create(), 0);

        // phyinfof("sizeof(%zu)", sizeof(tree_node_t<uint32_t, uint32_t, 6, 6>));
        // phyinfof("sizeof(%zu)", sizeof(tree_node_t<uint64_t, uint32_t, 6, 6>));
        // phyinfof("sizeof(%zu)", sizeof(tree_node_t<uint64_t, uint64_t, 6, 6>));

        uint32_t found = 0u;

        ASSERT_EQ(tree.find(1, found), 0);

        ASSERT_EQ(tree.add(1, 1), 0);
        ASSERT_EQ(tree.find(1, found), 1);
        ASSERT_EQ(found, 1u);

        ASSERT_EQ(tree.add(2, 2), 0);
        ASSERT_EQ(tree.find(2, found), 1);
        ASSERT_EQ(found, 2u);

        ASSERT_EQ(tree.add(3, 3), 0);
        ASSERT_EQ(tree.find(3, found), 1);
        ASSERT_EQ(found, 3u);

        ASSERT_EQ(tree.find(4, found), 0);
    });
}

TEST_F(TreeSuite, SingleNodeTreeGrowingByOneNode) {
    mounted([&](directory_chain &chain) {
        auto first = allocator().allocate();
        tree_sector tree{ dhara(), allocator(), simple_buffer{ sector_size() }, first, "tree" };

        ASSERT_EQ(tree.create(), 0);

        for (auto i = 1u; i < 8; ++i) {
            uint32_t found = 0u;
            ASSERT_EQ(tree.add(i, i), 0);
            EXPECT_EQ(tree.find(i, found), 1);
            ASSERT_EQ(found, i);
        }
    });
}

TEST_F(TreeSuite, SingleNodeTreeGrowingByTwoNodes) {
    mounted([&](directory_chain &chain) {
        auto first = allocator().allocate();
        tree_sector tree{ dhara(), allocator(), simple_buffer{ sector_size() }, first, "tree" };

        ASSERT_EQ(tree.create(), 0);

        for (auto i = 1u; i < 16; ++i) {
            uint32_t found = 0u;
            ASSERT_EQ(tree.add(i, i), 0);
            EXPECT_EQ(tree.find(i, found), 1);
            ASSERT_EQ(found, i);
        }

        for (auto i = 1u; i < 16; ++i) {
            uint32_t found = 0u;
            EXPECT_EQ(tree.find(i, found), 1);
            ASSERT_EQ(found, i);
        }
    });
}

TEST_F(TreeSuite, TreeWith1024Node1Reachable) {
    mounted([&](directory_chain &chain) {
        auto first = allocator().allocate();
        tree_sector tree{ dhara(), allocator(), simple_buffer{ sector_size() }, first, "tree" };

        ASSERT_EQ(tree.create(), 0);

        for (auto i = 1u; i < 1024; ++i) {
            ASSERT_EQ(tree.add(i, i), 0);

            uint32_t found = 0u;
            EXPECT_EQ(tree.find(1, found), 1);
            ASSERT_EQ(found, 1u);
        }
    });
}

TEST_F(TreeSuite, TreeAllReachableAsAdded) {
    mounted([&](directory_chain &chain) {
        auto first = allocator().allocate();
        tree_sector tree{ dhara(), allocator(), simple_buffer{ sector_size() }, first, "tree" };

        ASSERT_EQ(tree.create(), 0);

        for (auto i = 1u; i < 1024; ++i) {
            ASSERT_EQ(tree.add(i, i), 0);

            // Downgraded because of performance.
            if (i % 16 == 0) {
                for (auto j = 1u; j < i; ++j) {
                    uint32_t found = 0u;
                    EXPECT_EQ(tree.find(j, found), 1);
                    ASSERT_EQ(found, j);
                }
            }
        }
    });
}

TEST_F(TreeSuite, TreeWith1024) {
    mounted([&](directory_chain &chain) {
        auto first = allocator().allocate();
        tree_sector tree{ dhara(), allocator(), simple_buffer{ sector_size() }, first, "tree" };

        ASSERT_EQ(tree.create(), 0);

        for (auto i = 1u; i < 1024; ++i) {
            uint32_t found = 0u;
            ASSERT_EQ(tree.add(i, i), 0);
            ASSERT_EQ(tree.find(i, found), 1);
            ASSERT_EQ(found, i);
        }

        for (auto i = 1u; i < 1024; ++i) {
            uint32_t found = 0u;
            EXPECT_EQ(tree.find(i, found), 1);
            ASSERT_EQ(found, i);
        }
    });
}

TEST_F(TreeSuite, DISABLED_TreeWith10241024) {
    mounted([&](directory_chain &chain) {
        auto first = allocator().allocate();
        tree_sector tree{ dhara(), allocator(), simple_buffer{ sector_size() }, first, "tree" };

        ASSERT_EQ(tree.create(), 0);

        for (auto i = 1u; i < 1024 * 1024; ++i) {
            uint32_t found = 0u;
            ASSERT_EQ(tree.add(i, i), 0);
            EXPECT_EQ(tree.find(i, found), 1);
            ASSERT_EQ(found, i);
        }

        for (auto i = 1u; i < 1024; ++i) {
            uint32_t found = 0u;
            EXPECT_EQ(tree.find(i, found), 1);
            ASSERT_EQ(found, i);
        }
    });
}
