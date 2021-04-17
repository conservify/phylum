#include "string_format.h"

#include <directory_chain.h>
#include <directory_tree.h>
#include <file_appender.h>
#include <file_reader.h>
#include <super_chain.h>

#include "phylum_tests.h"
#include "geometry.h"

using namespace phylum;

template <typename T> class IndexedFixture : public PhylumFixture {};

template<typename directory_type, typename tree_type>
class file_ops {
private:
    super_chain &sc_;
    directory_type dir_;

public:
    file_ops(phyctx pc, super_chain &sc) : sc_(sc), dir_{ pc, sc.directory_tree() } {
    }

public:
    directory_type &dir() {
        return dir_;
    }

public:
    int32_t mount() {
        return 0;
    }

    int32_t format() {
        auto err = dir_.format();
        if (err < 0) {
            return err;
        }

        err = sc_.update(dir_.to_tree_ptr());
        if (err < 0) {
            return err;
        }

        return 0;
    }

    int32_t touch(const char *name) {
        auto err = dir_.template touch_indexed<tree_type>(name);
        if (err < 0) {
            return err;
        }

        err = sc_.update(dir_.to_tree_ptr());
        if (err < 0) {
            return err;
        }

        return 0;
    }

    int32_t update(file_appender &appender, record_number_t record_number) {
        return appender.index_if_necessary<tree_type>(record_number);
    }

};

template<typename TLayout, typename TDirectory, typename TTree>
struct test_types {
    using layout_type = TLayout;
    using directory_type = TDirectory;
    using tree_type = TTree;
};

typedef ::testing::Types<
    test_types<layout_4096, directory_tree, tree_sector<uint32_t, uint32_t, 63>>,
    test_types<layout_4096, directory_tree, tree_sector<uint32_t, uint32_t, 405>>>
    Implementations;

TYPED_TEST_SUITE(IndexedFixture, Implementations);

TYPED_TEST(IndexedFixture, TouchedIndexed) {
    using layout_type = typename TypeParam::layout_type;
    using directory_type = typename TypeParam::directory_type;
    using tree_type = typename TypeParam::tree_type;
    using file_ops_type = file_ops<directory_type, tree_type>;

    layout_type layout;
    FlashMemory memory{ layout.sector_size };

    memory.mounted<super_chain>([&](super_chain &super) {
        file_ops_type fops{ memory.pc(), super };
        ASSERT_EQ(fops.format(), 0);
        ASSERT_EQ(fops.touch("data.txt"), 0);
    });
}

TYPED_TEST(IndexedFixture, TouchedIndexed_10) {
    using layout_type = typename TypeParam::layout_type;
    using directory_type = typename TypeParam::directory_type;
    using tree_type = typename TypeParam::tree_type;
    using file_ops_type = file_ops<directory_type, tree_type>;

    layout_type layout;
    FlashMemory memory{ layout.sector_size };

    std::vector<std::string> files;

    memory.mounted<super_chain>([&](super_chain &super) {
        file_ops_type fops{ memory.pc(), super };
        ASSERT_EQ(fops.format(), 0);
    });

    for (auto i = 0; i < 10; ++i) {
        memory.mounted<super_chain>([&](super_chain &super) {
            std::string name = string_format("data-%d.txt", i);
            file_ops_type fops{ memory.pc(), super };
            ASSERT_EQ(fops.touch(name.c_str()), 0);
            files.push_back(name);
        });
    }

    memory.mounted<super_chain>([&](super_chain &super) {
        directory_type dir{ memory.pc(), super.directory_tree() };

        for (auto name : files) {
            ASSERT_EQ(dir.template find(name.c_str(), open_file_config{ }), 1);
        }
    });
}

TYPED_TEST(IndexedFixture, TouchedIndexed_100_FindAfterEach) {
    using layout_type = typename TypeParam::layout_type;
    using directory_type = typename TypeParam::directory_type;
    using tree_type = typename TypeParam::tree_type;
    using file_ops_type = file_ops<directory_type, tree_type>;

    layout_type layout;
    FlashMemory memory{ layout.sector_size };

    memory.mounted<super_chain>([&](super_chain &super) {
        file_ops_type fops{ memory.pc(), super };
        ASSERT_EQ(fops.format(), 0);
    });

    std::vector<std::string> files;

    for (auto i = 0; i < 100; ++i) {
        std::string name = string_format("data-%d.txt", i);

        phydebugf("touching %s", name.c_str());

        auto pass = true;

        memory.mounted<super_chain>([&](super_chain &super) {
            file_ops_type fops{ memory.pc(), super };
            ASSERT_EQ(fops.touch(name.c_str()), 0);
            ASSERT_EQ(fops.dir().find(name.c_str(), open_file_config{ }), 1);
        });

        memory.mounted<super_chain>([&](super_chain &super) {
            file_ops_type fops{ memory.pc(), super };
            ASSERT_EQ(fops.dir().find(name.c_str(), open_file_config{ }), 1);
        });

        ASSERT_TRUE(pass);

        files.push_back(name);
    }
}

TYPED_TEST(IndexedFixture, TouchedIndexed_100) {
    using layout_type = typename TypeParam::layout_type;
    using directory_type = typename TypeParam::directory_type;
    using tree_type = typename TypeParam::tree_type;
    using file_ops_type = file_ops<directory_type, tree_type>;

    layout_type layout;
    FlashMemory memory{ layout.sector_size };

    memory.mounted<super_chain>([&](super_chain &super) {
        file_ops_type fops{ memory.pc(), super };
        ASSERT_EQ(fops.format(), 0);
    });

    std::vector<std::string> files;

    for (auto i = 0; i < 100; ++i) {
        std::string name = string_format("data-%d.txt", i);
        phydebugf("touching %s", name.c_str());
        memory.mounted<super_chain>([&](super_chain &super) {
            file_ops_type fops{ memory.pc(), super };
            ASSERT_EQ(fops.touch(name.c_str()), 0);
            memory.buffers().debug();
        });
        files.push_back(name);
    }

    memory.mounted<super_chain>([&](super_chain &super) {
        directory_type dir{ memory.pc(), super.directory_tree() };
        for (auto name : files) {
            ASSERT_EQ(dir.find(name.c_str(), open_file_config{ }), 1);
        }
    });
}

TYPED_TEST(IndexedFixture, WriteFile_Large) {
    using layout_type = typename TypeParam::layout_type;
    using directory_type = typename TypeParam::directory_type;
    using tree_type = typename TypeParam::tree_type;
    using file_ops_type = file_ops<directory_type, tree_type>;

    layout_type layout;
    FlashMemory memory{ layout.sector_size };

    memory.mounted<super_chain>([&](super_chain &super) {
        file_ops_type fops{ memory.pc(), super };
        ASSERT_EQ(fops.format(), 0);
        ASSERT_EQ(fops.touch("data.txt"), 0);

        ASSERT_EQ(fops.dir().find("data.txt", open_file_config{ }), 1);
        file_appender opened{ memory.pc(), &fops.dir(), fops.dir().open() };

        auto written = 0u;
        auto record_number = 0u;
        {
            phydebugf("suppressing 1MB of writes from debug");
            // suppress_logs sl;

            while (written < 1024u * 1024u) {
                ASSERT_GE(fops.update(opened, record_number), 0);
                auto wrote = opened.write(lorem1k);
                ASSERT_GT(wrote, 0);
                written += wrote;
                record_number++;
            }

            ASSERT_EQ(opened.flush(), 0);
        }
    });
}
