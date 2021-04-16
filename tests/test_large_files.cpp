#include <directory_chain.h>
#include <file_appender.h>
#include <tree_sector.h>

#include "phylum_tests.h"
#include "geometry.h"

using namespace phylum;

template<typename T>
class LargeFileFixture : public PhylumFixture {};

typedef ::testing::Types<
    std::pair<layout_256, tree_sector<uint32_t, uint32_t, 5>>,
    std::pair<layout_4096, tree_sector<uint32_t, uint32_t, 63>>,
    std::pair<layout_4096, tree_sector<uint32_t, uint32_t, 287>>,
    std::pair<layout_4096, tree_sector<uint32_t, uint32_t, 405>>>
    Implementations;

TYPED_TEST_SUITE(LargeFileFixture, Implementations);

TYPED_TEST(LargeFileFixture, WriteOneMegabyte) {
    typename TypeParam::first_type layout;
    FlashMemory memory{ layout.sector_size };

    memory.mounted<directory_chain>([&](auto &chain) {
        ASSERT_EQ(chain.touch("data.txt"), 0);

        ASSERT_EQ(chain.find("data.txt", open_file_config{ }), 1);
        file_appender opened{ chain, &chain, chain.open() };

        phydebugf("suppressing 1MB of writes from debug");
        suppress_logs sl;

        auto written = 0u;
        while (written < 1024u * 1024u) {
            auto wrote = opened.write(lorem1k);
            ASSERT_GT(wrote, 0);
            written += wrote;
        }

        ASSERT_EQ(opened.flush(), 0);
    });

    memory.mounted<directory_chain>([&](auto &chain) {
        ASSERT_EQ(chain.log(), 0);
    });
}

TYPED_TEST(LargeFileFixture, WriteOneMegabyteIndexed) {
    typename TypeParam::first_type layout;
    FlashMemory memory{ layout.sector_size };

    memory.mounted<directory_chain>([&](auto &chain) {
        ASSERT_EQ(chain.touch("data.txt"), 0);

        ASSERT_EQ(chain.find("data.txt", open_file_config{ }), 1);
        file_appender opened{ chain, &chain, chain.open() };

        auto position_index_sector = memory.allocator().allocate();
        typename TypeParam::second_type position_index{ memory.buffers(), memory.sectors(), memory.allocator(), position_index_sector, "posidx" };

        auto record_index_sector = memory.allocator().allocate();
        typename TypeParam::second_type record_index{ memory.buffers(), memory.sectors(), memory.allocator(), record_index_sector, "recidx" };

        ASSERT_EQ(position_index.create(), 0);
        ASSERT_EQ(record_index.create(), 0);

        auto written = 0u;
        auto cursor = opened.cursor();
        auto record_number = 0u;
        {
            phydebugf("suppressing 1MB of writes from debug");
            suppress_logs sl;

            while (written < 1024u * 1024u) {
                if (opened.length_sectors() % 16 == 0) {
                    if (written == 0 || opened.cursor().sector != cursor.sector) {
                        cursor = opened.cursor();

                        log_configure_level(LogLevels::DEBUG);
                        phydebugf("cursor sector=%d position=%d position_atsos=%d record_number=%d",
                                  cursor.sector, cursor.position, cursor.position_at_start_of_sector, record_number);
                        log_configure_level(LogLevels::NONE);

                        ASSERT_EQ(position_index.add(cursor.position_at_start_of_sector, cursor.sector), 0);
                        ASSERT_EQ(record_index.add(record_number, cursor.position), 0);
                    }
                }

                auto wrote = opened.write(lorem1k);
                ASSERT_GT(wrote, 0);
                written += wrote;
                record_number++;
            }
            ASSERT_EQ(opened.flush(), 0);
        }

        uint32_t end = UINT32_MAX;
        uint32_t found_key = 0;
        uint32_t found_value = 0;
        ASSERT_EQ(position_index.find_last_less_then(end, &found_value, &found_key), 1);

        ASSERT_EQ(found_key, cursor.position_at_start_of_sector);
        ASSERT_EQ(found_value, cursor.sector);

        position_index.log();
    });
}
