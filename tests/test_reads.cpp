#include <directory_chain.h>
#include <file_appender.h>
#include <file_reader.h>

#include "suite_base.h"
#include "geometry.h"

using namespace phylum;

class ReadSuite : public PhylumSuite {
protected:
    FlashMemory memory{ 256 };

};

TEST_F(ReadSuite, ReadInlineWrite) {
    auto hello = "Hello, world! How are you?";

    memory.mounted([&](directory_chain &chain) {
        ASSERT_EQ(chain.touch("data.txt"), 0);
        ASSERT_EQ(chain.flush(), 0);

        ASSERT_EQ(chain.find("data.txt", file_cfg()), 1);
        simple_buffer file_buffer{ memory.sector_size() };
        file_appender opened{ chain, chain.open(), std::move(file_buffer) };
        ASSERT_GT(opened.write(hello), 0);
        ASSERT_EQ(opened.flush(), 0);
    });

    memory.mounted([&](directory_chain &chain) {
        ASSERT_EQ(chain.find("data.txt", file_cfg()), 1);
        simple_buffer file_buffer{ memory.sector_size() };
        file_reader reader{ chain, chain.open(), std::move(file_buffer) };

        uint8_t buffer[256];
        ASSERT_EQ(reader.read(buffer, sizeof(buffer)), (int32_t)strlen(hello));
        ASSERT_EQ(reader.position(), strlen(hello));
        ASSERT_EQ(reader.close(), 0);
    });
}

TEST_F(ReadSuite, ReadInlineWriteMultipleSameBlock) {
    auto hello = "Hello, world! How are you?";

    memory.mounted([&](directory_chain &chain) {
        ASSERT_EQ(chain.touch("data.txt"), 0);
        ASSERT_EQ(chain.flush(), 0);

        ASSERT_EQ(chain.find("data.txt", file_cfg()), 1);
        simple_buffer file_buffer{ memory.sector_size() };
        file_appender opened{ chain, chain.open(), std::move(file_buffer) };
        for (auto i = 0u; i < 3; ++i) {
            ASSERT_GT(opened.write(hello), 0);
        }
        ASSERT_EQ(opened.flush(), 0);
    });

    memory.mounted([&](directory_chain &chain) {
        ASSERT_EQ(chain.find("data.txt", file_cfg()), 1);
        simple_buffer file_buffer{ memory.sector_size() };
        file_reader reader{ chain, chain.open(), std::move(file_buffer) };

        uint8_t buffer[256];
        ASSERT_EQ(reader.read(buffer, sizeof(buffer)), (int32_t)strlen(hello) * 3);
        ASSERT_EQ(reader.position(), strlen(hello) * 3);
        ASSERT_EQ(reader.close(), 0);
    });
}

TEST_F(ReadSuite, ReadInlineWriteMultipleSeparateBlocks) {
    auto hello = "Hello, world! How are you?";

    memory.mounted([&](directory_chain &chain) {
        ASSERT_EQ(chain.touch("data.txt"), 0);
        ASSERT_EQ(chain.flush(), 0);

        for (auto i = 0u; i < 3; ++i) {
            ASSERT_EQ(chain.find("data.txt", file_cfg()), 1);
            simple_buffer file_buffer{ memory.sector_size() };
            file_appender opened{ chain, chain.open(), std::move(file_buffer) };
            ASSERT_GT(opened.write(hello), 0);
            ASSERT_EQ(opened.flush(), 0);
        }
    });

    memory.mounted([&](directory_chain &chain) {
        ASSERT_EQ(chain.find("data.txt", file_cfg()), 1);
        simple_buffer file_buffer{ memory.sector_size() };
        file_reader reader{ chain, chain.open(), std::move(file_buffer) };

        uint8_t buffer[256];
        ASSERT_EQ(reader.read(buffer, sizeof(buffer)), (int32_t)strlen(hello) * 3);
        ASSERT_EQ(reader.position(), strlen(hello) * 3);
        ASSERT_EQ(reader.close(), 0);
    });
}
