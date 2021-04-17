#pragma once

#include "sector_map.h"
#include "sector_allocator.h"
#include "delimited_buffer.h"
#include "working_buffers.h"
#include "paging_delimited_buffer.h"
#include "phyctx.h"

namespace phylum {

class Keys {
public:
    // Returns the position where 'key' should be inserted in a leaf node
    // that has the given keys.
    // NOTE: These and the following methods do a simple linear search, which is
    // just fine for N or M < 100. Any large and a Binary Search is better.
    template <typename KEY, typename NODE> static inline index_type leaf_position_for(const KEY &key, const NODE &node) {
        index_type k = 0;
        while ((k < node.number_keys) && (node.keys[k] < key)) {
            ++k;
        }
        assert(k <= node.number_keys);
        return k;
    }

    // Returns the position where 'key' should be inserted in an inner node
    // that has the given keys.
    template <typename KEY, typename NODE> static inline index_type inner_position_for(const KEY &key, const NODE &node) {
        index_type k = 0;
        while ((k < node.number_keys) && ((node.keys[k] < key) || (node.keys[k] == key))) {
            ++k;
        }
        return k;
    }
};

template <typename KEY, typename VALUE, size_t Size>
class tree_sector {
public:
    static constexpr size_t NodeSize = Size;
    using key_type = KEY;
    using value_type = VALUE;
    using default_node_type = tree_node_t<KEY, VALUE, Size>;

private:
    static constexpr size_t ScopeNameLength = 32;

    struct insertion_t {
        bool split{ false };
        KEY key{ 0 };
        node_ptr_t left;
        node_ptr_t right;
    };

    struct persisted_node_t {
        default_node_type *node{ nullptr };
        node_ptr_t ptr{};

        persisted_node_t() {
        }

        persisted_node_t(default_node_type *node, node_ptr_t ptr) : node(node), ptr(ptr) {
        }
    };

private:
    using buffer_type = paging_delimited_buffer;
    working_buffers *buffers_{ nullptr };
    sector_map *sectors_{ nullptr };
    sector_allocator *allocator_{ nullptr };
    buffer_type buffer_;
    dhara_sector_t root_{ InvalidSector };
    dhara_sector_t tail_{ InvalidSector };
    const char *prefix_{ "tree-sector" };
    char name_[ScopeNameLength];

public:
    tree_sector(phyctx pc, tree_ptr_t tree, const char *prefix = "tree")
        : buffers_(&pc.buffers_), sectors_(&pc.sectors_), allocator_(&pc.allocator_), buffer_(pc.buffers_, pc.sectors_), root_(tree.root), tail_(tree.tail), prefix_(prefix) {
        name("%s[%d]", prefix_, root_);
    }

    tree_sector(tree_sector &other, tree_ptr_t tree, const char *prefix)
        : buffers_(other.buffers_), sectors_(other.sectors_), allocator_(other.allocator_),
          buffer_(*other.buffers_, *other.sectors_), root_(tree.root), tail_(tree.tail), prefix_(prefix) {
        name("%s[%d]", prefix_, root_);
    }

    virtual ~tree_sector() {
    }

public:
    const char *name() const {
        return name_;
    }

    tree_ptr_t to_tree_ptr() const {
        return tree_ptr_t{ root_, tail_ };
    }

protected:
    size_t sector_size() const {
        return buffer_.size();
    }

    buffer_type &db() {
        return buffer_;
    }

    sector_allocator &allocator() {
        return *allocator_;
    }

private:
    static persisted_node_t find_sector_root(dhara_sector_t sector, delimited_buffer &db) {
        persisted_node_t selected;
        for (auto iter = db.begin(); iter != db.end(); ++iter) {
            if (iter->as<entry_t>()->type == entry_type::TreeNode) {
                auto node = db.as_mutable<default_node_type>(*iter);
                if (selected.node == nullptr || selected.node->depth < node->depth) {
                    selected = persisted_node_t{ node, node_ptr_t{ sector, (sector_offset_t)iter->position() } };
                }
            }
        }
        return selected;
    }

    static persisted_node_t find_node_in_sector(delimited_buffer &db, node_ptr_t ptr) {
        persisted_node_t selected;
        for (auto iter = db.begin(); iter != db.end(); ++iter) {
            if (iter->position() == ptr.position) {
                auto node = db.as_mutable<default_node_type>(*iter);
                return persisted_node_t{ node, ptr };
            }
        }
        return persisted_node_t{};
    }

private:
    void name(const char *f, ...) {
        va_list args;
        va_start(args, f);
        phy_vsnprintf(name_, sizeof(name_), f, args);
        va_end(args);
    }

    int32_t leaf_insert_nonfull(page_lock &lock, depth_type depth, node_ptr_t node_ptr, default_node_type *node, KEY &key, VALUE &value, unsigned index) {
        assert(node->type == node_type::Leaf);
        assert(node->number_keys < Size);
        assert(index < Size);
        assert(index <= node->number_keys);

        if (node->keys[index] == key) {
            // We are inserting a duplicate value. Simply overwrite the old one
            node->d.values[index] = value;
        } else {
            for (auto i = node->number_keys; i > index; --i) {
                node->keys[i] = node->keys[i - 1];
                node->d.values[i] = node->d.values[i - 1];
            }
            node->number_keys++;
            node->keys[index] = key;
            node->d.values[index] = value;
        }

        // assert(lock.sector() == node_ptr.sector);

        phydebugf("%s value node=%d:%d depth=%d", name(), node_ptr.sector, node_ptr.position, depth);

        lock.dirty();

        return 0;
    }

    int32_t leaf_node_insert(page_lock &lock, depth_type depth, node_ptr_t node_ptr, default_node_type *node, KEY &key, VALUE &value, insertion_t &insertion) {
        logged_task lt{ "leaf-node" };

        auto index = Keys::leaf_position_for(key, *node);

        if (node->number_keys >= Size) {
            phydebugf("node full, splitting");

            node_ptr_t sibling_ptr;
            return allocate_node(lock, sibling_ptr, [&](page_lock &new_lock, default_node_type *new_sibling, node_ptr_t new_sibling_ptr) -> int32_t {
                auto threshold = (Size + 1) / 2;
                new_sibling->type = node_type::Leaf;
                new_sibling->number_keys = node->number_keys - threshold;
                for (auto j = 0u; j < new_sibling->number_keys; ++j) {
                    new_sibling->keys[j] = node->keys[threshold + j];
                    new_sibling->d.values[j] = node->d.values[threshold + j];
                }

                new_lock.dirty();

                node->number_keys = threshold;

                lock.dirty();

                if (index < threshold) {
                    auto err = leaf_insert_nonfull(lock, depth - 1, node_ptr, node, key, value, index);
                    if (err < 0) {
                        return err;
                    }
                } else {
                    auto err = leaf_insert_nonfull(lock, depth - 1, new_sibling_ptr, new_sibling, key, value, index - threshold);
                    if (err < 0) {
                        return err;
                    }
                }

                insertion.split = true;
                insertion.key = new_sibling->keys[0];
                insertion.left = node_ptr;
                insertion.right = sibling_ptr;

                return 0;
            });
        } else {
            phydebugf("node ok, inserting");

            // NOTE Why this +1?
            return leaf_insert_nonfull(lock, depth + 1, node_ptr, node, key, value, index);
        }
    }

    int32_t inner_insert_nonfull(page_lock &lock, depth_type depth, node_ptr_t node_ptr, default_node_type *node, KEY &key, VALUE &value) {
        logged_task lt{ "inner-nonfull" };

        assert(node->type == node_type::Inner);
        assert(node->number_keys < Size);
        assert(depth != 0);

        auto left = lock.sector();

        auto index = Keys::inner_position_for(key, *node);
        auto child_ptr = node->d.children[index];

        phydebugf("%s entered depth=%d dirty=%d node-ptr=%d:%d follow=%d:%d", name(), depth, lock.is_dirty(),
                  node_ptr.sector, node_ptr.position, child_ptr.sector, child_ptr.position);

        persisted_node_t followed;
        auto err = follow_node_ptr(lock, child_ptr, followed);
        if (err < 0) {
            return err;
        }

        insertion_t insertion;
        if (depth - 1 == 0) {
            // Children are leaf.
            err = leaf_node_insert(lock, depth - 1, followed.ptr, followed.node, key, value, insertion);
            if (err < 0) {
                return err;
            }
        } else {
            // Children are inner.
            err = inner_node_insert(lock, depth - 1, followed.ptr, followed.node, key, value, insertion);
            if (err < 0) {
                return err;
            }
        }

        if (left != lock.sector()) {
            phydebugf("reloading previous=%d page-lock: sector=%d dirty=%d", left, lock.sector(), lock.is_dirty());

            auto err = flush(lock);
            if (err < 0) {
                return err;
            }

            err = lock.replace(left);
            if (err < 0) {
                return err;
            }
        }
        else {
            phydebugf("same sector %d", left);
        }

        // After the above we can end up with a different page in the
        // buffer and so we need to find the node we were on again. We
        // should fail due to other reasons before here.
        auto relocated = find_node_in_sector(db(), node_ptr);
        assert(relocated.node != nullptr);

        node = relocated.node;

        assert(node->number_keys < Size);

        if (insertion.split) {
            assert(index <= node->number_keys);
            if (index == node->number_keys) {
                // Insertion at the rightmost key
                node->keys[index] = insertion.key;
                node->d.children[index] = insertion.left;
                node->d.children[index + 1] = insertion.right;
                node->number_keys++;
            } else {
                // Insertion not at the rightmost key
                node->d.children[node->number_keys + 1] = node->d.children[node->number_keys];
                for (auto i = node->number_keys; i != index; --i) {
                    node->d.children[i] = node->d.children[i - 1];
                    node->keys[i] = node->keys[i - 1];
                }
                node->d.children[index] = insertion.left;
                node->d.children[index + 1] = insertion.right;
                node->keys[index] = insertion.key;
                node->number_keys++;
            }

            phydebugf("recording insertion, dirty %d:%d", node_ptr.sector, node_ptr.position);

            lock.dirty();
        }

        return 0;
    }

    int32_t inner_node_insert(page_lock &lock, depth_type depth, node_ptr_t node_ptr, default_node_type *node, KEY &key, VALUE &value, insertion_t &insertion) {
        logged_task lt{ "inner-node" };

        assert(node->type == node_type::Inner);
        assert(depth != 0);

        // Early split if node is full.
        // This is not the canonical algorithm for B+ trees,
        // but it is simpler and does not break the definition.
        if (node->number_keys == Size) {
            node_ptr_t ignored_ptr;
            auto err = allocate_node(lock, ignored_ptr, [&](page_lock &new_lock, default_node_type *new_sibling, node_ptr_t new_sibling_ptr) -> int32_t {
                auto treshold = (Size + 1) / 2;

                new_sibling->depth = depth;
                new_sibling->type = node_type::Inner;
                new_sibling->number_keys = node->number_keys - treshold;
                for (auto i = 0; i < new_sibling->number_keys; ++i) {
                    new_sibling->keys[i] = node->keys[treshold + i];
                    new_sibling->d.children[i] = node->d.children[treshold + i];
                }

                new_sibling->d.children[new_sibling->number_keys] = node->d.children[node->number_keys];

                new_lock.dirty();

                node->number_keys = treshold - 1;

                lock.dirty();

                // Set up the return variable
                insertion.split = true;
                insertion.key = node->keys[treshold - 1];
                insertion.left = node_ptr;
                insertion.right = new_sibling_ptr;

                // Now insert in the appropriate sibling
                if (key < insertion.key) {
                    auto err = inner_insert_nonfull(lock, depth, node_ptr, node, key, value);
                    if (err < 0) {
                        return err;
                    }
                } else {
                    auto err = inner_insert_nonfull(lock, depth, new_sibling_ptr, new_sibling, key, value);
                    if (err < 0) {
                        return err;
                    }
                }

                return 0;
            });
            if (err < 0) {
                return err;
            }
        } else {
            auto err = inner_insert_nonfull(lock, depth, node_ptr, node, key, value);
            if (err < 0) {
                return err;
            }
        }

        return 0;
    }

    int32_t flush(page_lock &lock) {
        auto err = lock.flush(lock.sector());
        if (err < 0) {
            return err;
        }

        return 0;
    }

    template<typename TFill>
    int32_t allocate_node(page_lock &lock, node_ptr_t &ptr, TFill fill_fn) {
        db().seek_end();

        if (db().template room_for<default_node_type>()) {
            phydebugf("%s appending node %d:%d", name(), lock.sector(), db().position());

            auto placed = db().template reserve<default_node_type>();

            ptr = node_ptr_t{ lock.sector(), placed.position };

            phydebugf("allocate-node filling");

            auto err = fill_fn(lock, placed.record, ptr);
            if (err < 0) {
                return err;
            }

            lock.dirty();

            phydebugf("allocate-node done filling");

            return 0;
        }

        auto allocated = allocator_->allocate();

        phydebugf("%s grow! %zu/%zu alloc=%d", name(), db().position(), db().size(), allocated);

        buffer_type buffer{ *buffers_, *sectors_ };

        auto child_lock = buffer.overwrite(allocated);

        buffer.template emplace<sector_chain_header_t>(entry_type::TreeSector, InvalidSector, tail_);

        tail_ = allocated;

        auto placed = buffer.template reserve<default_node_type>();

        phydebugf("creating new node %d:%d node-size=%d sector-size=%d",
                  allocated, buffer.position(), sizeof(default_node_type), db().size());

        ptr = node_ptr_t{ allocated, placed.position };

        phydebugf("allocate-node filling");

        auto err = fill_fn(child_lock, placed.record, ptr);
        if (err < 0) {
            return err;
        }

        phydebugf("allocate-node done filling");

        if (child_lock.is_dirty()) {
            phydebugf("allocate-node sector is dirty");
            err = child_lock.flush(allocated);
            if (err < 0) {
                return err;
            }
        }
        else {
            phydebugf("allocate-node leaving");
        }

        return 0;
    }

    int32_t follow_node_ptr(page_lock &lock, node_ptr_t &ptr, persisted_node_t &followed) {
        if (ptr.sector != lock.sector()) {
            phyinfof("follow %d -> %d:%d (load-sector)", lock.sector(), ptr.sector, ptr.position);

            // TODO Move this into replace.
            if (lock.is_dirty()) {
                auto err = lock.flush(lock.sector());
                if (err < 0) {
                    return err;
                }
            }

            auto err = lock.replace(ptr.sector);
            if (err < 0) {
                return err;
            }

            assert(lock.sector() == ptr.sector);

            phydebugf("follow %d:%d (done) page-lock-sector=%d", ptr.sector, ptr.position, lock.sector());
        } else {
            phyinfof("follow %d -> %d:%d (same-sector)", lock.sector(), ptr.sector, ptr.position);
        }

        followed = find_node_in_sector(db(), ptr);
        if (followed.node == nullptr) {
            phyerrorf("follow: unable to find node in sector");
            return -1;
        }

        return 0;
    }

    int32_t back_to_root(page_lock &lock) {
        phydebugf("%s back-to-root %d -> %d", name(), lock.sector(), root_);

        assert(!lock.is_dirty());

        auto err = lock.replace(root_);
        if (err < 0) {
            return err;
        }

        db().rewind();

        return 0;
    }

    int32_t log(page_lock &lock, default_node_type *node) {
        logged_task it{ name() };

        if (node->type == node_type::Inner) {
            for (auto i = 0u; i < node->number_keys; ++i) {
                auto child = node->d.children[i];

                if (false) {
                    phyinfof("inner %d #%d key=%d -> %d:%d", lock.sector(), i, node->keys[i], child.sector, child.position);
                }
                else {
                    if (lock.sector() != child.sector) {
                        phyinfof("sector %d -> %d", lock.sector(), child.sector);
                    }
                }

                auto left = lock.sector();

                persisted_node_t followed;
                auto err = follow_node_ptr(lock, child, followed);
                if (err < 0) {
                    return err;
                }

                err = log(lock, followed.node);
                if (err < 0) {
                    return err;
                }

                if (left != lock.sector()) {
                    phydebugf("reloading %d", left);

                    auto err = lock.replace(left);
                    if (err < 0) {
                        return err;
                    }
                }
            }
        }

        return 0;
    }

public:
    int32_t exists() {
        logged_task lt{ "tree-exists" };

        dhara_page_t page = 0;
        auto err = sectors_->find(root_, &page);
        if (err < 0) {
            return 0;
        }

        phydebugf("exists!");

        return 1;
    }

    int32_t create() {
        logged_task lt{ "tree-create" };

        phydebugf("%s creating", name());

        auto lock = db().overwrite(root_);

        assert(db().empty());

        db().template emplace<sector_chain_header_t>(entry_type::TreeSector);

        phydebugf("creating new tree position=%d node-size=%d sector-size=%d", db().position(), sizeof(default_node_type), db().size());

        db().template emplace<default_node_type>(node_type::Leaf);

        lock.dirty();

        auto err = flush(lock);
        if (err < 0) {
            return err;
        }

        return 0;
    }

    int32_t add(KEY key, VALUE value) {
        logged_task lt{ "tree-add" };

        phydebugf("%s adding node", name());

        auto lock = db().writing(root_);

        auto err = back_to_root(lock);
        if (err < 0) {
            return err;
        }

        auto pnode = find_sector_root(lock.sector(), db());
        assert(pnode.node != nullptr);

        auto node = pnode.node;
        auto node_ptr = pnode.ptr;

        phydebugf("%s adding node depth=%d", name(), node->depth);

        insertion_t insertion;

        if (node->depth == 0) {
            auto err = leaf_node_insert(lock, node->depth, node_ptr, node, key, value, insertion);
            if (err < 0) {
                return err;
            }
        } else {
            auto err = inner_node_insert(lock, node->depth, node_ptr, node, key, value, insertion);
            if (err < 0) {
                return err;
            }
        }

        if (insertion.split) {
            assert(lock.sector() == root_);

            phydebugf("allocating new root new-depth=%d", node->depth + 1);

            // The old root was separated in two parts.
            // We have to create a new root pointing to them
            node_ptr_t ptr;
            auto err = allocate_node(lock, ptr, [&](page_lock &new_lock, default_node_type *new_node, node_ptr_t /*ignored_ptr*/) {
                new_node->type = node_type::Inner;
                new_node->depth = node->depth + 1;
                new_node->number_keys = 1;
                new_node->keys[0] = insertion.key;
                new_node->d.children[0] = insertion.left;
                new_node->d.children[1] = insertion.right;
                new_lock.dirty();
                return 0;
            });
            if (err < 0) {
                return err;
            }

            root_ = ptr.sector;

            phydebugf("%s new root", name());
        }

        err = flush(lock);
        if (err < 0) {
            return err;
        }

        phydebugf("%s done adding", name());

        return 0;
    }

    int32_t find(KEY key, VALUE *value = 0) {
        logged_task lt{ "tree-find" };

        phydebugf("finding %d", key);

        auto lock = db().reading(root_);

        auto err = back_to_root(lock);
        if (err < 0) {
            return err;
        }

        auto pnode = find_sector_root(lock.sector(), db());
        auto node = pnode.node;
        assert(node != nullptr);

        auto starting_depth = node->depth;
        auto d = starting_depth;
        while (d-- != 0 && node->type == node_type::Inner) {
            auto index = Keys::inner_position_for(key, *node);
            assert(index < node->number_keys + 1);

            auto child_ptr = node->d.children[index];
            persisted_node_t followed;
            auto err = follow_node_ptr(lock, child_ptr, followed);
            if (err < 0) {
                return err;
            }

            node = followed.node;
        }

        assert(node->type == node_type::Leaf);
        auto index = Keys::leaf_position_for(key, *node);
        assert(index <= node->number_keys);
        if (index < node->number_keys && node->keys[index] == key) {
            if (value != nullptr) {
                *value = node->d.values[index];
            }
            return 1;
        }

        return 0;
    }

    bool find_last_less_then(const KEY &key, VALUE *value = 0, KEY *out_key = 0) {
        logged_task lt{ "tree-find-less" };

        phydebugf("finding %d", key);

        auto lock = db().reading(root_);

        auto err = back_to_root(lock);
        if (err < 0) {
            return err;
        }

        auto pnode = find_sector_root(lock.sector(), db());
        auto node = pnode.node;
        assert(node != nullptr);


        auto starting_depth = node->depth;
        auto d = starting_depth;
        while (d-- != 0 && node->type == node_type::Inner) {
            auto index = Keys::inner_position_for(key, *node);
            assert(index < node->number_keys + 1);

            if (index > 0 && key == node->keys[index - 1]) {
                index -= 1;
            }
            assert(index == 0 || node->keys[index - 1] < key);

            auto child_ptr = node->d.children[index];
            persisted_node_t followed;
            auto err = follow_node_ptr(lock, child_ptr, followed);
            if (err < 0) {
                return err;
            }

            node = followed.node;
        }

        assert(node->type == node_type::Leaf);
        auto index = Keys::leaf_position_for(key, *node);

        if (index <= node->number_keys) {
            index -= 1;
            if (index < node->number_keys && key == node->keys[index]) {
                index -= 1;
            }

            if (index < node->number_keys) {
                assert(node->keys[index] < key);

                if (value != nullptr) {
                    *value = node->d.values[index];
                }
                if (out_key != nullptr) {
                    *out_key = node->keys[index];
                }

                return 1;
            }
        }

        return 0;
    }

    int32_t log() {
        logged_task lt{ "tree-log" };

        auto lock = db().reading(root_);

        auto err = back_to_root(lock);
        if (err < 0) {
            return err;
        }

        auto pnode = find_sector_root(lock.sector(), db());
        auto node = pnode.node;
        assert(node != nullptr);

        return log(lock, node);
    }

    int32_t dequeue_sector(dhara_sector_t *sector) {
        assert(sector != nullptr);
        *sector = InvalidSector;
        return 0;
    }

};

} // namespace phylum
