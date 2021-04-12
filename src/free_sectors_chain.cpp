#include "free_sectors_chain.h"

namespace phylum {

free_sectors_chain::free_sectors_chain(working_buffers &buffers, sector_map &sectors, sector_allocator &allocator,
                                       head_tail_t chain, const char *prefix)
    : record_chain(buffers, sectors, allocator, chain, prefix) {
}

free_sectors_chain::free_sectors_chain(sector_chain &other, head_tail_t chain, const char *prefix)
    : record_chain(other, chain, prefix) {
}

free_sectors_chain::~free_sectors_chain() {
}

int32_t free_sectors_chain::add_free_sectors(free_sectors_t record) {
    assert(record.head != InvalidSector);

    logged_task lt{ "fc-add-chain" };

    auto page_lock = db().writing(sector());

    assert(back_to_head(page_lock) >= 0);

    while (true) {
        for (auto iter = db().begin(); iter != db().end(); ++iter) {
            auto entry = iter->as<entry_t>();
            assert(entry != nullptr);

            if (entry->type == entry_type::FreeSectors) {
                auto fs = iter->as<free_sectors_t>();

                if (fs->head == InvalidSector) {
                    phydebugf("add-chain: reusing: %d", fs->head);

                    auto mutable_record = db().as_mutable<free_sectors_t>(*iter);

                    *mutable_record = record;

                    page_lock.dirty();

                    auto err = page_lock.flush(this->sector());
                    if (err < 0) {
                        return err;
                    }

                    return 0;
                }
            }
        }

        auto err = forward(page_lock);
        if (err < 0) {
            return err;
        }
        if (err == 0) {
            break;
        }
    }

    assert(append<free_sectors_t>(page_lock, record, nullptr, 0u) >= 0);

    auto err = flush(page_lock);
    if (err < 0) {
        return err;
    }

    return 0;
}

int32_t free_sectors_chain::add_chain(dhara_sector_t head) {
    return add_free_sectors(free_sectors_t{ head });
}

int32_t free_sectors_chain::add_tree(tree_ptr_t tree, size_t tree_size) {
    return add_free_sectors(free_sectors_t{ tree.tail, tree_size });
}

class free_sectors_tree {
private:
    using buffer_type = paging_delimited_buffer;
    working_buffers *buffers_{ nullptr };
    sector_map *sectors_{ nullptr };
    buffer_type buffer_;
    dhara_sector_t root_{ InvalidSector };
    size_t size_{ 0 };

public:
    free_sectors_tree(working_buffers &buffers, sector_map &sectors, dhara_sector_t root, size_t size)
        : buffers_(&buffers), sectors_(&sectors), buffer_(buffers, sectors), root_(root), size_(size) {
    }

public:
    dhara_sector_t root() {
        return root_;
    }

    buffer_type &db() {
        return buffer_;
    }

private:
    int32_t dequeue_leaf(page_lock &page_lock, dhara_sector_t sector, dhara_sector_t *dequeued) {
        auto leaf = true;

        auto err = page_lock.replace(sector);
        if (err < 0) {
            return err;
        }

        for (auto iter = db().begin(); iter != db().end(); ++iter) {
            auto node = iter->as<tree_node_header_t>();
            phydebugf("node:iter depth=%d nkeys=%d type=%d", node->depth, node->number_keys, node->type);

            if (node->type != node_type::Leaf) {
                auto child_ptr = (node_ptr_t *)((uint8_t *)node + sizeof(tree_node_header_t));
                for (auto i = 0u; i <= node->number_keys; ++i) {
                    if (child_ptr->sector != InvalidSector && child_ptr->sector != sector) {
                        leaf = false;

                        auto err = page_lock.flush(sector);
                        if (err < 0) {
                            return err;
                        }

                        err = dequeue_leaf(page_lock, child_ptr->sector, dequeued);
                        if (err < 0) {
                            return err;
                        }

                        err = page_lock.replace(sector);
                        if (err < 0) {
                            return err;
                        }
                    }

                    if (*dequeued != InvalidSector && child_ptr->sector == *dequeued) {
                        auto mutable_node = db().as_mutable<tree_node_header_t>(*iter);
                        auto mutable_children = (node_ptr_t *)((uint8_t *)mutable_node + sizeof(tree_node_header_t));

                        mutable_children[i] = node_ptr_t{};

                        page_lock.dirty();
                    }

                    child_ptr++;
                }
            }
        }

        err = page_lock.flush(sector);
        if (err < 0) {
            return err;
        }

        if (leaf && *dequeued == InvalidSector) {
            *dequeued = sector;
            if (root_ == sector) {
                root_ = InvalidSector;
            }
        }

        if (*dequeued == InvalidSector) {
            return 0;
        }

        return 1;
    }

public:
    int32_t dequeue_sector(dhara_sector_t *dequeued) {
        assert(dequeued != nullptr);
        *dequeued = InvalidSector;

        auto page_lock = db().writing(root_);

        auto err = dequeue_leaf(page_lock, root_, dequeued);
        if (err < 0) {
            return err;
        }

        return err;
    }

};

int32_t free_sectors_chain::dequeue(dhara_sector_t *sector) {
    assert(sector != nullptr);

    *sector = InvalidSector;

    auto page_lock = db().writing(head());

    assert(back_to_head(page_lock) >= 0);

    while (true) {
        for (auto iter = db().begin(); iter != db().end(); ++iter) {
            auto entry = iter->as<entry_t>();
            assert(entry != nullptr);

            if (entry->type == entry_type::FreeSectors) {
                auto fs = iter->as<free_sectors_t>();
                auto new_head = fs->head;

                int32_t err = 0;

                if (fs->head != InvalidSector && fs->tree_size == 0) {
                    phydebugf("walk: free-sectors(chain): %d", fs->head);

                    free_sectors_chain chain{ *this, head_tail_t{ fs->head, InvalidSector }, "dequeue" };
                    err = chain.dequeue_sector(sector);
                    if (err < 0) {
                        return err;
                    }

                    new_head = chain.head();
                }

                if (fs->head != InvalidSector && fs->tree_size != 0) {
                    phydebugf("walk: free-sectors(tree): %d", fs->head);

                    free_sectors_tree fst{ buffers(), *sectors(), fs->head, fs->tree_size };
                    err = fst.dequeue_sector(sector);
                    if (err < 0) {
                        return err;
                    }

                    new_head = fst.root();
                }

                if (err > 0) {
                    if (fs->head != new_head) {
                        phydebugf("walk: head changed %d vs %d", fs->head, new_head);

                        auto mutable_record = db().as_mutable<free_sectors_t>(*iter);

                        mutable_record->head = new_head;

                        page_lock.dirty();

                        err = page_lock.flush(this->sector());
                        if (err < 0) {
                            return err;
                        }
                    }

                    return 1;
                }
            }
        }

        auto err = forward(page_lock);
        if (err < 0) {
            return err;
        }
        if (err == 0) {
            break;
        }
    }

    return 0;
}

int32_t free_sectors_chain::write_header(page_lock &page_lock) {
    logged_task lt{ "fc-write-hdr", this->name() };

    assert_valid();

    db().emplace<free_chain_header_t>();

    page_lock.dirty();
    appendable(true);

    return 0;
}

int32_t free_sectors_chain::seek_end_of_buffer(page_lock & /*page_lock*/) {
    auto err = db().seek_end();
    if (err < 0) {
        return err;
    }

    appendable(true);

    return 0;
}

} // namespace phylum
