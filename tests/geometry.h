#pragma once

namespace phylum {

class sector_geometry {
private:
    working_buffers &buffers_;
    sector_map &map_;

public:
    sector_geometry(working_buffers &buffers, sector_map &map) : buffers_(buffers), map_(map) {
    }

public:
    class verify_sector {
    private:
        dhara_sector_t sector_;
        delimited_buffer buffer_;

    public:
        verify_sector(working_buffers &buffers, sector_map &map, dhara_sector_t sector) : sector_(sector), buffer_(std::move(buffers.allocate(map.sector_size()))) {
            buffer_.unsafe_all([&](uint8_t *ptr, size_t size) {
                auto err = map.read(sector_, ptr, size);
                // phydebug_dump_memory("verify ", ptr, size);
                return err;
            });
        }

    public:
        testing::AssertionResult end(size_t n) {
            auto iter = buffer_.begin();
            while (n > 0) {
                ++iter;
                --n;
            }
            if (iter == buffer_.end()) {
                return testing::AssertionSuccess();
            }
            return testing::AssertionFailure() << "Unexpected end of records";
        }

        template <typename T>
        testing::AssertionResult nth(size_t n, T expected) {
            return nth<T>(n, expected, nullptr, 0);
        }

        template <typename T>
        testing::AssertionResult nth(size_t n, T expected, uint32_t data) {
            return nth<T>(n, expected, (uint8_t *)&data, sizeof(uint32_t));
        }

        template <typename T>
        testing::AssertionResult nth(size_t n, T expected, uint8_t const *expected_payload, size_t payload_size) {
            auto iter = buffer_.begin();
            while (n > 0) {
                ++iter;
                if (iter == buffer_.end()) {
                    return testing::AssertionFailure() << "Unexpected end of records";
                }
                --n;
            }
            auto actual = iter->as<T>();
            if (memcmp(&expected, actual, sizeof(T)) == 0) {
                if (payload_size > 0) {
                    auto err = iter->data<T>([&](uint8_t *ptr, size_t size) {
                        if (memcmp(expected_payload, ptr, size) == 0) {
                            return 0;
                        }
                        phydebug_dump_memory("payload-actual   ", (uint8_t *)ptr, size);
                        phydebug_dump_memory("payload-expected ", (uint8_t *)expected_payload, payload_size);
                        return -1;
                    });
                    if (err < 0) {
                        return testing::AssertionFailure() << "Header record";
                    }
                    return testing::AssertionSuccess();
                }
                return testing::AssertionSuccess();
            }
            phydebug_dump_memory("actual   ", (uint8_t *)actual, sizeof(T));
            phydebug_dump_memory("expected ", (uint8_t *)&expected, sizeof(T));
            return testing::AssertionFailure() << "Header record";
        }

        template <typename T>
        testing::AssertionResult header(T expected) {
            return nth(0, expected);
        }
    };

public:
    verify_sector sector(dhara_sector_t sector) {
        return verify_sector{ buffers_, map_, sector };
    }
};

} // namespace phylum
