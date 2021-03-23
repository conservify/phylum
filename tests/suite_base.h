#pragma once

#include <gtest/gtest.h>

#include <memory_dhara_map.h>
#include <directory_chain.h>

#include "fake_data.h"
#include "geometry.h"

using namespace phylum;

constexpr uint8_t ATTRIBUTE_ONE = 0x1;
constexpr uint8_t ATTRIBUTE_TWO = 0x2;

class PhylumSuite : public ::testing::Test {
private:
    size_t sector_size_{ 256 };
    memory_dhara_map dhara_{ sector_size() };
    bool formatted_{ false };
    open_file_attribute attributes_[2] = {
        { .type = ATTRIBUTE_ONE, .size = 4, .ptr = nullptr },
        { .type = ATTRIBUTE_TWO, .size = 4, .ptr = nullptr }
    };
    open_file_config file_cfg_;

public:
    size_t sector_size() const {
        return sector_size_;
    }

    dhara_map &dhara() {
        return dhara_;
    }

    open_file_config &file_cfg() {
        return file_cfg_;
    }

public:
    PhylumSuite() {
    }

    virtual ~PhylumSuite() {
    }

public:
    void SetUp() override {
        dhara_.clear();
        formatted_ = false;

        file_cfg_ = {
            .attributes = attributes_,
            .nattrs = 2,
        };
        for (auto &attr : attributes_) {
            attr.ptr = malloc(attr.size);
        }
    }

    void TearDown() override {
        for (auto &attr : attributes_) {
            free(attr.ptr);
        }
    }

public:
    template<typename T>
    void sync(T fn) {
        fn();
    }

    template<typename T>
    void mounted(T fn) {
        sector_allocator allocator{ dhara_ };
        directory_chain chain{ dhara_, allocator, 0, simple_buffer{ sector_size() } };
        if (formatted_) {
            ASSERT_EQ(chain.mount(), 0);
        }
        else {
            ASSERT_EQ(chain.format(), 0);
            formatted_ = true;
        }
        fn(chain);
    }
};
