#include <directory_chain.h>
#include <file_appender.h>

#include "suite_base.h"
#include "geometry.h"

using namespace phylum;

class LargeFileSuite : public PhylumSuite {};

TEST_F(LargeFileSuite, WriteOneMegabyte) {
    mounted([&](directory_chain &chain) {
        ASSERT_EQ(chain.touch("data.txt"), 0);
        ASSERT_EQ(chain.flush(), 0);

        ASSERT_EQ(chain.find("data.txt", file_cfg()), 1);
        simple_buffer file_buffer{ sector_size() };
        file_appender opened{ chain, chain.open(), std::move(file_buffer) };

        auto written = 0u;
        while (written < 1024u * 1024u) {
            auto wrote = opened.write(lorem1k);
            ASSERT_GT(wrote, 0);
            written += wrote;
        }
        ASSERT_EQ(opened.flush(), 0);
    });

    mounted([&](directory_chain &chain) {
        ASSERT_EQ(chain.log(), 0);
    });
}
