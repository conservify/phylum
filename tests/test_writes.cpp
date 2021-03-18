#include <directory_chain.h>
#include <file_appender.h>

#include "suite_base.h"
#include "geometry.h"

using namespace phylum;

class WriteSuite : public PhylumSuite {};

TEST_F(WriteSuite, WriteInlineOnce) {
    mounted([&](directory_chain &chain) {
        ASSERT_EQ(chain.touch("data.txt"), 0);
        ASSERT_EQ(chain.flush(), 0);

        ASSERT_EQ(chain.find("data.txt", file_cfg()), 1);
        simple_buffer file_buffer{ sector_size() };
        file_appender opened{ chain, chain.open(), std::move(file_buffer) };

        auto hello = "Hello, world! How are you!";

        ASSERT_GT(opened.write(hello), 0);
        ASSERT_EQ(opened.flush(), 0);

        sector_geometry sg{ dhara() };
        EXPECT_TRUE(sg.sector(0).header<directory_chain_header_t>({ }));
        EXPECT_TRUE(sg.sector(0).nth<file_entry_t>(1, { "data.txt" }));
        EXPECT_TRUE(sg.sector(0).nth<file_data_t>(2, { make_file_id("data.txt"), (file_size_t)strlen(hello) }));
        EXPECT_TRUE(sg.sector(0).end(3));
    });
}

TEST_F(WriteSuite, WriteInlineBuffersMultipleSmall) {
    mounted([&](directory_chain &chain) {
        ASSERT_EQ(chain.touch("data.txt"), 0);
        ASSERT_EQ(chain.flush(), 0);

        ASSERT_EQ(chain.find("data.txt", file_cfg()), 1);
        simple_buffer file_buffer{ sector_size() };
        file_appender opened{ chain, chain.open(), std::move(file_buffer) };

        auto hello = "Hello, world! How are you!";

        ASSERT_GT(opened.write(hello), 0);
        ASSERT_GT(opened.write(hello), 0);
        ASSERT_GT(opened.write(hello), 0);
        ASSERT_EQ(opened.flush(), 0);

        sector_geometry sg{ dhara() };
        EXPECT_TRUE(sg.sector(0).header<directory_chain_header_t>({ }));
        EXPECT_TRUE(sg.sector(0).nth<file_entry_t>(1, { "data.txt" }));
        EXPECT_TRUE(sg.sector(0).nth<file_data_t>(2, { make_file_id("data.txt"), (file_size_t)strlen(hello) * 3 }));
        EXPECT_TRUE(sg.sector(0).end(3));
    });
}

TEST_F(WriteSuite, WriteInlineMultipleFlushEach) {
    mounted([&](directory_chain &chain) {
        ASSERT_EQ(chain.touch("data.txt"), 0);
        ASSERT_EQ(chain.flush(), 0);

        ASSERT_EQ(chain.find("data.txt", file_cfg()), 1);
        simple_buffer file_buffer{ sector_size() };
        file_appender opened{ chain, chain.open(), std::move(file_buffer) };

        auto hello = "Hello, world! How are you!";

        ASSERT_GT(opened.write(hello), 0);
        ASSERT_EQ(opened.flush(), 0);
        ASSERT_GT(opened.write(hello), 0);
        ASSERT_EQ(opened.flush(), 0);
        ASSERT_GT(opened.write(hello), 0);
        ASSERT_EQ(opened.flush(), 0);

        sector_geometry sg{ dhara() };
        EXPECT_TRUE(sg.sector(0).header<directory_chain_header_t>({ }));
        EXPECT_TRUE(sg.sector(0).nth<file_entry_t>(1, { "data.txt" }));
        EXPECT_TRUE(sg.sector(0).nth<file_data_t>(2, { make_file_id("data.txt"), (file_size_t)strlen(hello) }));
        EXPECT_TRUE(sg.sector(0).nth<file_data_t>(3, { make_file_id("data.txt"), (file_size_t)strlen(hello) }));
        EXPECT_TRUE(sg.sector(0).nth<file_data_t>(4, { make_file_id("data.txt"), (file_size_t)strlen(hello) }));
        EXPECT_TRUE(sg.sector(0).end(5));
    });
}

TEST_F(WriteSuite, WriteThreeInlineWritesAndTriggerDataChain) {
    mounted([&](directory_chain &chain) {
        ASSERT_EQ(chain.touch("data.txt"), 0);
        ASSERT_EQ(chain.flush(), 0);

        ASSERT_EQ(chain.find("data.txt", file_cfg()), 1);
        simple_buffer file_buffer{ sector_size() };
        file_appender opened{ chain, chain.open(), std::move(file_buffer) };

        auto hello = "Hello, world! How are you!";

        ASSERT_GT(opened.write(hello), 0);
        ASSERT_EQ(opened.flush(), 0);
        ASSERT_GT(opened.write(hello), 0);
        ASSERT_EQ(opened.flush(), 0);
        ASSERT_GT(opened.write(hello), 0);
        ASSERT_EQ(opened.flush(), 0);
        ASSERT_GT(opened.write(lorem1k, sector_size() / 2 + 8), 0);
        ASSERT_EQ(opened.flush(), 0);

        sector_geometry sg{ dhara() };
        EXPECT_TRUE(sg.sector(0).header<directory_chain_header_t>({ }));
        EXPECT_TRUE(sg.sector(0).nth<file_entry_t>(1, { "data.txt" }));
        EXPECT_TRUE(sg.sector(0).nth<file_data_t>(2, { make_file_id("data.txt"), (file_size_t)strlen(hello) }));
        EXPECT_TRUE(sg.sector(0).nth<file_data_t>(3, { make_file_id("data.txt"), (file_size_t)strlen(hello) }));
        EXPECT_TRUE(sg.sector(0).nth<file_data_t>(4, { make_file_id("data.txt"), (file_size_t)strlen(hello) }));
        EXPECT_TRUE(sg.sector(0).nth<file_data_t>(5, { make_file_id("data.txt"), head_tail_t{ 1, 1 } }));
        EXPECT_TRUE(sg.sector(0).end(6));

        EXPECT_TRUE(sg.sector(1).header<data_chain_header_t>({ (uint16_t)((strlen(hello) * 3) + (sector_size() / 2 + 8)) }));
        EXPECT_TRUE(sg.sector(1).end(1));
    });
}

TEST_F(WriteSuite, WriteAppendsToDataChain) {
    mounted([&](directory_chain &chain) {
        ASSERT_EQ(chain.touch("data.txt"), 0);
        ASSERT_EQ(chain.flush(), 0);

        ASSERT_EQ(chain.find("data.txt", file_cfg()), 1);
        simple_buffer file_buffer{ sector_size() };
        file_appender opened{ chain, chain.open(), std::move(file_buffer) };

        auto hello = "Hello, world! How are you!";

        ASSERT_GT(opened.write(hello), 0);
        ASSERT_EQ(opened.flush(), 0);
        ASSERT_GT(opened.write(hello), 0);
        ASSERT_EQ(opened.flush(), 0);
        ASSERT_GT(opened.write(hello), 0);
        ASSERT_EQ(opened.flush(), 0);
        ASSERT_GT(opened.write(lorem1k, sector_size() / 2 + 8), 0);
        ASSERT_EQ(opened.flush(), 0);
        ASSERT_GT(opened.write(hello), 0);
        ASSERT_EQ(opened.flush(), 0);

        sector_geometry sg{ dhara() };
        EXPECT_TRUE(sg.sector(0).header<directory_chain_header_t>({ }));
        EXPECT_TRUE(sg.sector(0).nth<file_entry_t>(1, { "data.txt" }));
        EXPECT_TRUE(sg.sector(0).nth<file_data_t>(2, { make_file_id("data.txt"), (file_size_t)strlen(hello) }));
        EXPECT_TRUE(sg.sector(0).nth<file_data_t>(3, { make_file_id("data.txt"), (file_size_t)strlen(hello) }));
        EXPECT_TRUE(sg.sector(0).nth<file_data_t>(4, { make_file_id("data.txt"), (file_size_t)strlen(hello) }));
        EXPECT_TRUE(sg.sector(0).nth<file_data_t>(5, { make_file_id("data.txt"), head_tail_t{ 1, 1 } }));
        EXPECT_TRUE(sg.sector(0).end(6));

        EXPECT_TRUE(sg.sector(1).header<data_chain_header_t>({ (uint16_t)((strlen(hello) * 4) + (sector_size() / 2 + 8)) }));
        EXPECT_TRUE(sg.sector(1).end(1));
    });
}

TEST_F(WriteSuite, WriteAppendsToDataChainGrowingToNewBlock) {
    mounted([&](directory_chain &chain) {
        ASSERT_EQ(chain.touch("data.txt"), 0);
        ASSERT_EQ(chain.flush(), 0);

        ASSERT_EQ(chain.find("data.txt", file_cfg()), 1);
        simple_buffer file_buffer{ sector_size() };
        file_appender opened{ chain, chain.open(), std::move(file_buffer) };

        auto hello = "Hello, world! How are you!";

        ASSERT_GT(opened.write(hello), 0);
        ASSERT_EQ(opened.flush(), 0);
        ASSERT_GT(opened.write(hello), 0);
        ASSERT_EQ(opened.flush(), 0);
        ASSERT_GT(opened.write(hello), 0);
        ASSERT_EQ(opened.flush(), 0);
        ASSERT_GT(opened.write(lorem1k, sector_size() / 2 + 8), 0);
        ASSERT_EQ(opened.flush(), 0);
        ASSERT_GT(opened.write(hello, sector_size()), 0);
        ASSERT_EQ(opened.flush(), 0);

        sector_geometry sg{ dhara() };
        EXPECT_TRUE(sg.sector(0).header<directory_chain_header_t>({ }));
        EXPECT_TRUE(sg.sector(0).nth<file_entry_t>(1, { "data.txt" }));
        EXPECT_TRUE(sg.sector(0).nth<file_data_t>(2, { make_file_id("data.txt"), (file_size_t)strlen(hello) }));
        EXPECT_TRUE(sg.sector(0).nth<file_data_t>(3, { make_file_id("data.txt"), (file_size_t)strlen(hello) }));
        EXPECT_TRUE(sg.sector(0).nth<file_data_t>(4, { make_file_id("data.txt"), (file_size_t)strlen(hello) }));
        EXPECT_TRUE(sg.sector(0).nth<file_data_t>(5, { make_file_id("data.txt"), head_tail_t{ 1, 1 } }));
        EXPECT_TRUE(sg.sector(0).end(6));

        EXPECT_TRUE(sg.sector(1).header<data_chain_header_t>({ (uint16_t)242, InvalidSector, 2 }));
        EXPECT_TRUE(sg.sector(1).end(1));

        EXPECT_TRUE(sg.sector(2).header<data_chain_header_t>({ (uint16_t)228, 1, InvalidSector }));
        EXPECT_TRUE(sg.sector(2).end(1));
    });
}

TEST_F(WriteSuite, WriteAndIncrementAttribute) {
    mounted([&](directory_chain &chain) {
        ASSERT_EQ(chain.touch("data.txt"), 0);
        ASSERT_EQ(chain.flush(), 0);

        ASSERT_EQ(chain.find("data.txt", file_cfg()), 1);
        simple_buffer file_buffer{ sector_size() };
        file_appender opened{ chain, chain.open(), std::move(file_buffer) };

        auto hello = "Hello, world! How are you!";

        for (auto i = 0u; i < 3; ++i) {
            opened.u32(ATTRIBUTE_ONE, opened.u32(ATTRIBUTE_ONE) + 1);
            ASSERT_GT(opened.write(hello), 0);
            ASSERT_GE(opened.flush(), 0);
        }

        ASSERT_GE(opened.close(), 0);

        sector_geometry sg{ dhara() };
        EXPECT_TRUE(sg.sector(0).header<directory_chain_header_t>({ }));
        EXPECT_TRUE(sg.sector(0).nth<file_entry_t>(1, { "data.txt" }));
        EXPECT_TRUE(sg.sector(0).nth<file_data_t>(2, { make_file_id("data.txt"), (file_size_t)strlen(hello) }));
        EXPECT_TRUE(sg.sector(0).nth<file_data_t>(3, { make_file_id("data.txt"), (file_size_t)strlen(hello) }));
        EXPECT_TRUE(sg.sector(0).nth<file_data_t>(4, { make_file_id("data.txt"), (file_size_t)strlen(hello) }));
        EXPECT_TRUE(sg.sector(0).nth<file_attribute_t>(5, { make_file_id("data.txt"), ATTRIBUTE_ONE, sizeof(uint32_t) }, 3));
        EXPECT_TRUE(sg.sector(0).end(6));
    });
}

TEST_F(WriteSuite, WriteAndIncrementAttributeThreeTimes) {
    mounted([&](directory_chain &chain) {
        ASSERT_EQ(chain.touch("data.txt"), 0);
        ASSERT_EQ(chain.flush(), 0);

        ASSERT_EQ(chain.find("data.txt", file_cfg()), 1);
        simple_buffer file_buffer{ sector_size() };
        file_appender opened{ chain, chain.open(), std::move(file_buffer) };

        auto hello = "Hello, world! How are you!";

        for (auto i = 0u; i < 3; ++i) {
            opened.u32(ATTRIBUTE_ONE, opened.u32(ATTRIBUTE_ONE) + 1);
            ASSERT_GT(opened.write(hello), 0);
            ASSERT_GE(opened.flush(), 0);
            ASSERT_GE(opened.close(), 0);
        }

        sector_geometry sg{ dhara() };
        EXPECT_TRUE(sg.sector(0).header<directory_chain_header_t>({ }));
        EXPECT_TRUE(sg.sector(0).nth<file_entry_t>(1, { "data.txt" }));
        EXPECT_TRUE(sg.sector(0).nth<file_data_t>(2, { make_file_id("data.txt"), (file_size_t)strlen(hello) }));
        EXPECT_TRUE(sg.sector(0).nth<file_attribute_t>(3, { make_file_id("data.txt"), ATTRIBUTE_ONE, sizeof(uint32_t) }, 1));
        EXPECT_TRUE(sg.sector(0).nth<file_data_t>(4, { make_file_id("data.txt"), (file_size_t)strlen(hello) }));
        EXPECT_TRUE(sg.sector(0).nth<file_attribute_t>(5, { make_file_id("data.txt"), ATTRIBUTE_ONE, sizeof(uint32_t) }, 2));
        EXPECT_TRUE(sg.sector(0).nth<file_data_t>(6, { make_file_id("data.txt"), (file_size_t)strlen(hello) }));
        EXPECT_TRUE(sg.sector(0).nth<file_attribute_t>(7, { make_file_id("data.txt"), ATTRIBUTE_ONE, sizeof(uint32_t) }, 3));
        EXPECT_TRUE(sg.sector(0).end(8));
    });
}

TEST_F(WriteSuite, WriteToDataChainAndIncrementAttributeThreeTimes) {
    mounted([&](directory_chain &chain) {
        ASSERT_EQ(chain.touch("data.txt"), 0);
        ASSERT_EQ(chain.flush(), 0);

        ASSERT_EQ(chain.find("data.txt", file_cfg()), 1);
        simple_buffer file_buffer{ sector_size() };
        file_appender opened{ chain, chain.open(), std::move(file_buffer) };

        auto hello = "Hello, world! How are you!";

        for (auto i = 0u; i < 3; ++i) {
            opened.u32(ATTRIBUTE_ONE, opened.u32(ATTRIBUTE_ONE) + 1);
            ASSERT_GT(opened.write(hello), 0);
            ASSERT_GE(opened.flush(), 0);
            ASSERT_GE(opened.close(), 0);
        }

        for (auto i = 0u; i < 2; ++i) {
            opened.u32(ATTRIBUTE_ONE, opened.u32(ATTRIBUTE_ONE) + 1);
            ASSERT_GT(opened.write(lorem1k, sector_size() / 2 + 8), 0);
            ASSERT_EQ(opened.flush(), 0);
            ASSERT_GE(opened.close(), 0);
        }

        sector_geometry sg{ dhara() };
        EXPECT_TRUE(sg.sector(0).header<directory_chain_header_t>({ InvalidSector, 2 }));
        EXPECT_TRUE(sg.sector(0).nth<file_entry_t>(1, { "data.txt" }));
        EXPECT_TRUE(sg.sector(0).nth<file_data_t>(2, { make_file_id("data.txt"), (file_size_t)strlen(hello) }));
        EXPECT_TRUE(sg.sector(0).nth<file_attribute_t>(3, { make_file_id("data.txt"), ATTRIBUTE_ONE, sizeof(uint32_t) }, 1));
        EXPECT_TRUE(sg.sector(0).nth<file_data_t>(4, { make_file_id("data.txt"), (file_size_t)strlen(hello) }));
        EXPECT_TRUE(sg.sector(0).nth<file_attribute_t>(5, { make_file_id("data.txt"), ATTRIBUTE_ONE, sizeof(uint32_t) }, 2));
        EXPECT_TRUE(sg.sector(0).nth<file_data_t>(6, { make_file_id("data.txt"), (file_size_t)strlen(hello) }));
        EXPECT_TRUE(sg.sector(0).nth<file_attribute_t>(7, { make_file_id("data.txt"), ATTRIBUTE_ONE, sizeof(uint32_t) }, 3));
        EXPECT_TRUE(sg.sector(0).end(8));

        EXPECT_TRUE(sg.sector(2).header<directory_chain_header_t>({ 0, InvalidSector }));
        EXPECT_TRUE(sg.sector(2).nth<file_data_t>(1, { make_file_id("data.txt"), head_tail_t{ 1, 1 } }));
        EXPECT_TRUE(sg.sector(2).nth<file_attribute_t>(2, { make_file_id("data.txt"), ATTRIBUTE_ONE, sizeof(uint32_t) }, 4));
        EXPECT_TRUE(sg.sector(2).nth<file_attribute_t>(3, { make_file_id("data.txt"), ATTRIBUTE_ONE, sizeof(uint32_t) }, 5));
        EXPECT_TRUE(sg.sector(2).end(4));

        EXPECT_TRUE(sg.sector(1).header<data_chain_header_t>({ (uint16_t)242, InvalidSector, 3 }));
        EXPECT_TRUE(sg.sector(1).end(1));

        EXPECT_TRUE(sg.sector(3).header<data_chain_header_t>({ (uint16_t)108, 1, InvalidSector }));
        EXPECT_TRUE(sg.sector(3).end(1));
    });
}
