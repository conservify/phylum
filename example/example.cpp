#include "task_stack.h"
#include "varint.h"

#include <algorithm>
#include <cassert>
#include <cinttypes>
#include <cstdarg>
#include <cstring>
#include <iostream>

#include "memory_dhara_map.h"
#include "directory_chain.h"
#include "data_chain.h"
#include "file_appender.h"

using namespace phylum;

int32_t main() {
    auto sector_size = 256u;

    memory_dhara_map dhara{ sector_size };
    sector_allocator allocator{ dhara };

    if (false) {
        directory_chain chain{ dhara, allocator, 0, simple_buffer{ sector_size } };
        chain.mount();
        chain.touch("test.logs");
        chain.flush();

        if (false) {
            head_tail_t sectors_chain;
            integer_chain sectors{ chain, sectors_chain };
            assert(sectors.create_if_necessary() >= 0);

            uint32_t values[100];
            for (auto i = 0; i < 100; ++i) {
                values[i] = 243329 + i * 34 % 54654;
            }

            assert(sectors.write(values, 100) >= 0);
            assert(sectors.flush() >= 0);
        }

        if (false) {
            head_tail_t sectors_chain{ 1, 2 };
            integer_chain sectors{ chain, sectors_chain };

            assert(sectors.back_to_head() >= 0);

            simple_buffer data{ 256 };
            while (true) {
                auto nread = sectors.read((uint32_t *)data.ptr(), data.size() / sizeof(uint32_t));
                debugf("nread: %d\n", nread);
                if (nread == 0) {
                    break;
                } else if (nread < 0) {
                    return -1;
                }
            }
        }
    }

    if (true) {
        directory_chain chain{ dhara, allocator, 0, simple_buffer{ sector_size } };
        chain.format();
        chain.touch("hello.txt");
        chain.touch("world.txt");
        chain.touch("1.txt");
        chain.touch("2.txt");
        chain.touch("3.txt");
        chain.touch("4.txt");
        chain.flush();
    }

    if (true) {
        directory_chain chain{ dhara, allocator, 0, simple_buffer{ sector_size } };
        chain.mount();
        chain.log();
    }

    open_file_attribute attributes[2] = {
        { .type = 0x01, .size = 4, .ptr = malloc(4) },
        { .type = 0x02, .size = 4, .ptr = malloc(4) }
    };
    open_file_config file_cfg = {
        .attributes = attributes,
        .nattrs = 1,
    };

    if (true) {
        debugf("************ writing to file\n");

        directory_chain chain{ dhara, allocator, 0, simple_buffer{ sector_size } };
        assert(chain.mount() <= 0);
        assert(chain.find("3.txt", file_cfg) >= 0);

        simple_buffer file_buffer{ sector_size };
        file_appender opened{ chain, chain.open(), std::move(file_buffer) };
        auto writing = "Hello, world!";
        if (opened.write((uint8_t const *)writing, strlen(writing)) < 0) {
            debugf("write error\n");
            return 0;
        } else {
            if (opened.close() < 0) {
                debugf("close error\n");
                return 0;
            }
        }
    }

    if (true) {
        debugf("************ appending to file (once)\n");

        directory_chain chain{ dhara, allocator, 0, simple_buffer{ 256 } };
        assert(chain.mount() <= 0);
        assert(chain.find("3.txt", file_cfg) >= 0);

        simple_buffer file_buffer{ sector_size };
        file_appender opened{ chain, chain.open(), std::move(file_buffer) };
        auto writing = "Hello, world!";
        if (opened.write((uint8_t const *)writing, strlen(writing)) < 0) {
            debugf("write error\n");
            return 0;
        } else {
            opened.u32(0x01, 1);
            if (opened.close() < 0) {
                debugf("close error\n");
                return 0;
            }
        }
    }

    if (true) {
        debugf("************ appending to file (10)\n");

        directory_chain chain{ dhara, allocator, 0, simple_buffer{ sector_size } };
        assert(chain.mount() <= 0);
        assert(chain.find("3.txt", file_cfg) >= 0);

        for (auto i = 0u; i < 10; ++i) {
            simple_buffer file_buffer{ sector_size };
            file_appender opened{ chain, chain.open(), std::move(file_buffer) };
            auto writing = "Hello, world!";
            assert(opened.write((uint8_t const *)writing, strlen(writing)) >= 0);
            opened.u32(0x01, opened.u32(0x01) + 1);
            assert(opened.close() >= 0);
        }
    }

    if (true) {
        debugf("************ appending to file (chunk)\n");

        for (auto k = 0u; k < 2; ++k) {
            directory_chain chain{ dhara, allocator, 0, simple_buffer{ sector_size } };
            assert(chain.mount() <= 0);
            assert(chain.find("3.txt", file_cfg) >= 0);

            simple_buffer file_buffer{ sector_size };
            file_appender opened{ chain, chain.open(), std::move(file_buffer) };

            simple_buffer chunk{ sector_size };
            for (auto i = 0u; i < sector_size; ++i) {
                chunk.ptr()[i] = i;
            }

            for (auto i = 0u; i < 2; ++i) {
                assert(opened.write(chunk.ptr(), chunk.size()) >= 0);
            }

            opened.u32(0x01, opened.u32(0x01) + 1);

            assert(opened.close() >= 0);
        }
    }

    if (true) {
        directory_chain chain{ dhara, allocator, 0, simple_buffer{ sector_size } };
        assert(chain.mount() <= 0);
        assert(chain.find("3.txt", open_file_config{ }) >= 0);

        data_chain sectors{ chain, chain.file().chain };
        assert(sectors.back_to_head() >= 0);

        simple_buffer data{ 256 };
        while (true) {
            auto nread = sectors.read(data.ptr(), data.size());
            debugf("nread: %d\n", nread);
            // fk_dump_memory("read: ", data.ptr(), nread);
            if (nread == 0) {
                break;
            } else if (nread < 0) {
                return -1;
            }
        }
    }

    if (true) {
        debugf("************ walking\n");

        directory_chain chain{ dhara, allocator, 0, simple_buffer{ sector_size } };
        assert(chain.mount() <= 0);
        chain.log();
    }

    for (auto &attr : attributes) {
        free(attr.ptr);
    }

    return 0;
}
