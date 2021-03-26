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

TEST_F(ReadSuite, ReadDataChain_TwoBlocks) {
    auto hello = "Hello, world! How are you!";

    memory.mounted([&](directory_chain &chain) {
        ASSERT_EQ(chain.touch("data.txt"), 0);
        ASSERT_EQ(chain.flush(), 0);

        ASSERT_EQ(chain.find("data.txt", file_cfg()), 1);
        simple_buffer file_buffer{ memory.sector_size() };
        file_appender opened{ chain, chain.open(), std::move(file_buffer) };

        for (auto i = 0u; i < 10; ++i) {
            ASSERT_GT(opened.write(hello), 0);
        }
        ASSERT_EQ(opened.flush(), 0);
    });

    memory.mounted([&](directory_chain &chain) {
        ASSERT_EQ(chain.find("data.txt", file_cfg()), 1);
        simple_buffer file_buffer{ memory.sector_size() };
        file_reader reader{ chain, chain.open(), std::move(file_buffer) };

        uint8_t buffer[256];
        ASSERT_EQ(reader.read(buffer, sizeof(buffer)), 242);

        ASSERT_EQ(reader.position(), 242u);
        ASSERT_EQ(reader.read(buffer, sizeof(buffer)), 18);
        ASSERT_EQ(reader.position(), (size_t)(strlen(hello) * 10));
        ASSERT_EQ(reader.close(), 0);
    });
}

TEST_F(ReadSuite, ReadDataChain_SeveralBlocks) {
    auto hello = "Hello, world! How are you!";

    memory.mounted([&](directory_chain &chain) {
        ASSERT_EQ(chain.touch("data.txt"), 0);
        ASSERT_EQ(chain.flush(), 0);

        ASSERT_EQ(chain.find("data.txt", file_cfg()), 1);
        simple_buffer file_buffer{ memory.sector_size() };
        file_appender opened{ chain, chain.open(), std::move(file_buffer) };

        for (auto i = 0u; i < 100; ++i) {
            ASSERT_GT(opened.write(hello), 0);
        }
        ASSERT_EQ(opened.flush(), 0);
    });

    memory.mounted([&](directory_chain &chain) {
        ASSERT_EQ(chain.find("data.txt", file_cfg()), 1);
        simple_buffer file_buffer{ memory.sector_size() };
        file_reader reader{ chain, chain.open(), std::move(file_buffer) };

        uint8_t buffer[256];
        ASSERT_EQ(reader.read(buffer, sizeof(buffer)), 242);
        ASSERT_EQ(reader.position(), 242u * 1);
        ASSERT_EQ(reader.read(buffer, sizeof(buffer)), 242);
        ASSERT_EQ(reader.position(), 242u * 2);
        ASSERT_EQ(reader.read(buffer, sizeof(buffer)), 242);
        ASSERT_EQ(reader.position(), 242u * 3);
        ASSERT_EQ(reader.read(buffer, sizeof(buffer)), 242);
        ASSERT_EQ(reader.position(), 242u * 4);
        ASSERT_EQ(reader.read(buffer, sizeof(buffer)), 242);
        ASSERT_EQ(reader.position(), 242u * 5);
        ASSERT_EQ(reader.read(buffer, sizeof(buffer)), 242);
        ASSERT_EQ(reader.position(), 242u * 6);
        ASSERT_EQ(reader.read(buffer, sizeof(buffer)), 242);
        ASSERT_EQ(reader.position(), 242u * 7);
        ASSERT_EQ(reader.read(buffer, sizeof(buffer)), 242);
        ASSERT_EQ(reader.position(), 242u * 8);
        ASSERT_EQ(reader.read(buffer, sizeof(buffer)), 242);
        ASSERT_EQ(reader.position(), 242u * 9);
        ASSERT_EQ(reader.read(buffer, sizeof(buffer)), 242);
        ASSERT_EQ(reader.position(), 242u * 10);
        ASSERT_EQ(reader.read(buffer, sizeof(buffer)), 180);
        ASSERT_EQ(reader.position(), 2600u);
        ASSERT_EQ(reader.close(), 0);
    });
}
