#include "binary_search_tree.h"
#include "memory_layout_generator.h"
#include <assert.h>
#include <iso646.h>
#include <stdalign.h>
#include <stdbool.h>
#include <stdint.h>

#define BST_RED   0
#define BST_BLACK 1

typedef struct bst_location bst_location_t;

struct bst_location {
    jfs_bst_node_t *node;
    jfs_bst_node_t *parent;
    int             parent_cmp;
};

static void           *bst_container_value(const jfs_bst_t *tree, const jfs_bst_node_t *node) WUR;
static jfs_bst_node_t *bst_container_node(const jfs_bst_t *tree, const void *value);
static void           *bst_pack_node(const jfs_bst_t *tree, jfs_bst_node_t *node, const void *value);
static void            bst_unpack_node(const jfs_bst_t *tree, const jfs_bst_node_t *node, void *value_out);
static void            bst_attach_node(jfs_bst_node_t *base_node, jfs_bst_node_t *attach_node);
static jfs_bst_node_t *bst_detach_node(jfs_bst_node_t *base_node);

static jfs_bst_node_t *bst_check_cache(const jfs_bst_t *tree, const void *key) WUR;
static bst_location_t  bst_find(const jfs_bst_t *tree, const void *key);
static void            bst_insert(jfs_bst_t *tree, jfs_bst_node_t *node, const bst_location_t *location);
static void            bst_delete(jfs_bst_t *tree, jfs_bst_node_t *node);
static void            bst_detach_and_delete(jfs_bst_t *tree, void *value_out, jfs_bst_node_t *node);

static void bst_update_cache_insert(jfs_bst_t *tree, const bst_location_t *location);
static void bst_update_cache_delete(jfs_bst_t *tree, const jfs_bst_node_t *node);

static void bst_fixup_insert(jfs_bst_t *tree, jfs_bst_node_t *node);
static void bst_fixup_delete(jfs_bst_t *tree, jfs_bst_node_t *node);

static void            bst_rotate_left(jfs_bst_t *tree, jfs_bst_node_t *x);
static void            bst_rotate_right(jfs_bst_t *tree, jfs_bst_node_t *x);
static void            bst_transplant(jfs_bst_t *tree, jfs_bst_node_t *old, jfs_bst_node_t *new);
static jfs_bst_node_t *bst_local_minimum(const jfs_bst_t *tree, jfs_bst_node_t *start);
static jfs_bst_node_t *bst_local_maximum(const jfs_bst_t *tree, jfs_bst_node_t *start);

static jfs_bst_node_t *bst_parent(const jfs_bst_node_t *node);
static void            bst_set_parent(jfs_bst_node_t *node, jfs_bst_node_t *parent);
static uint64_t        bst_color(const jfs_bst_node_t *node);
static void            bst_set_color(jfs_bst_node_t *node, uint64_t color);
static void            bst_set_parent_color(jfs_bst_node_t *node, jfs_bst_node_t *parent, uint64_t color);

jfs_mlg_desc_t jfs_bst_make_desc(size_t obj_size, size_t obj_align, size_t obj_count, jfs_err_t *err) {
    jfs_mlg_desc_t node_desc = {.align = alignof(jfs_bst_node_t), .size = sizeof(jfs_bst_node_t), .count = 1};
    assert(jfs_mlg_valid_desc(&node_desc));

    const jfs_mlg_desc_t obj_desc = {.align = obj_align, .size = obj_size, .count = obj_count};
    VAL_FAIL_IF(!jfs_mlg_valid_desc(&obj_desc), JFS_ERR_ARG, (jfs_mlg_desc_t) {0});

    jfs_mlg_append(&node_desc, &obj_desc); // we can reverse out the offset so we don't save it
    return node_desc;
}

void jfs_bst_init(jfs_bst_t *tree_init, const jfs_bst_conf_t *conf, jfs_err_t *err) {
    assert(conf != NULL);

    jfs_fl_init(&tree_init->free_list, conf->component, err);
    VOID_CHECK_ERR;

    tree_init->nil = &tree_init->nil_storage;
    tree_init->nil_storage.left = tree_init->nil;
    tree_init->nil_storage.right = tree_init->nil;
    bst_set_parent_color(tree_init->nil, tree_init->nil, BST_BLACK);

    tree_init->cache.largest = tree_init->nil;
    tree_init->cache.smallest = tree_init->nil;
    tree_init->cache.previous = tree_init->nil;
    tree_init->root = tree_init->nil;

    tree_init->value_offset = jfs_mlg_align_size(sizeof(jfs_bst_node_t), conf->component->desc.align);
    tree_init->value_size = conf->component->desc.size - tree_init->value_offset;

    tree_init->cmp = conf->cmp;
    VOID_FAIL_IF(tree_init->cmp == NULL, JFS_ERR_BAD_CONF);
}

void jfs_bst_puts(jfs_bst_t *tree, const void *value, const void *key, jfs_err_t *err) {
    VOID_FAIL_IF(tree->free_list.count == 0, JFS_ERR_FULL);
    jfs_bst_node_t *const node = jfs_fl_alloc(&tree->free_list);
    void *const           packed_value = bst_pack_node(tree, node, value);

    // look to see if the key matches a node in the cache
    jfs_bst_node_t *const cached_node = bst_check_cache(tree, key);
    if (cached_node != tree->nil) {
        tree->attach(bst_container_value(tree, cached_node), packed_value);
        tree->cache.previous = cached_node;
        return;
    }

    // find the node in the tree
    bst_location_t location = bst_find(tree, key);

    if (location.node != tree->nil) { // key is a dupe
        tree->attach(bst_container_value(tree, location.node), packed_value);
    } else { // key is not a dupe
        bst_insert(tree, node, &location);
        bst_update_cache_insert(tree, &location);
    }

    tree->cache.previous = node; // since it was a cache miss we update this cache
}

void jfs_bst_takes(jfs_bst_t *tree, void *value_out, const void *key, jfs_err_t *err) {
    VOID_FAIL_IF(tree->root == tree->nil, JFS_ERR_EMPTY);

    jfs_bst_node_t *const cached_node = bst_check_cache(tree, key);
    if (cached_node != tree->nil) {
        bst_unpack_node(tree, cached_node, value_out);
        return; // we found the node in the cache so we are done
    }

    bst_location_t location = bst_find(tree, key);
    VOID_FAIL_IF(location.node == tree->nil, JFS_ERR_BST_BAD_KEY);

    bst_detach_and_delete(tree, value_out, location.node);
}

void jfs_bst_get_largest(jfs_bst_t *tree, void *value_out, jfs_err_t *err) {
    VOID_FAIL_IF(tree->cache.largest == tree->nil, JFS_ERR_EMPTY);
    bst_detach_and_delete(tree, value_out, tree->cache.largest);
}

void jfs_bst_get_smallest(jfs_bst_t *tree, void *value_out, jfs_err_t *err) {
    VOID_FAIL_IF(tree->cache.smallest == tree->nil, JFS_ERR_EMPTY);
    bst_detach_and_delete(tree, value_out, tree->cache.smallest);
}

static void *bst_container_value(const jfs_bst_t *tree, const jfs_bst_node_t *node) {
    return jfs_mlg_apply_offset((uint8_t *) node, tree->value_offset);
}

static jfs_bst_node_t *bst_container_node(const jfs_bst_t *tree, const void *value) {
    return (jfs_bst_node_t *) (((uint8_t *) value) - tree->value_offset);
}

static void *bst_pack_node(const jfs_bst_t *tree, jfs_bst_node_t *node, const void *value) {
    void *const value_ptr = bst_container_value(tree, node);
    memcpy(value_ptr, value, tree->value_size);
    return value_ptr;
}

static void bst_unpack_node(const jfs_bst_t *tree, const jfs_bst_node_t *node, void *value_out) {
    void *const value_ptr = bst_container_value(tree, node);
    memcpy(value_out, value_ptr, tree->value_size);
}

static void bst_attach_node(jfs_bst_node_t *base_node, jfs_bst_node_t *attach_node) {
    attach_node->list = base_node->list;
    base_node->list = attach_node;
}
static jfs_bst_node_t *bst_detach_node(jfs_bst_node_t *base_node) {
    if (base_node->list == NULL) return base_node;

    jfs_bst_node_t *ret = base_node->list;
    base_node->list = base_node->list->list;
    return ret;
}

static jfs_bst_node_t *bst_check_cache(const jfs_bst_t *tree, const void *key) {
    if (tree->cache.previous != tree->nil) {
        const void *const previous_value = bst_container_value(tree, tree->cache.previous);
        if (tree->cmp(key, previous_value) == 0) return tree->cache.previous;
    }

    if (tree->cache.largest != tree->nil) {
        const void *const largest_value = bst_container_value(tree, tree->cache.largest);
        if (tree->cmp(key, largest_value) == 0) return tree->cache.largest;
    }

    if (tree->cache.smallest != tree->nil) {
        const void *const smallest_value = bst_container_value(tree, tree->cache.smallest);
        if (tree->cmp(key, smallest_value) == 0) return tree->cache.smallest;
    }

    return tree->nil;
}

static bst_location_t bst_find(const jfs_bst_t *tree, const void *key) {
    assert(tree != NULL);
    assert(key != NULL);

    bst_location_t location = {.node = tree->root, .parent = tree->nil, .parent_cmp = 0};

    while (location.node != tree->nil) {
        const void *node_value = bst_container_value(tree, location.node);
        const int   cmp_result = tree->cmp(key, node_value);

        // we found a matching node
        if (cmp_result == 0) return location;

        location.parent = location.node;
        location.parent_cmp = cmp_result;

        if (cmp_result < 0) {
            location.node = location.node->left;
        } else { // cmp_result > 0
            location.node = location.node->right;
        }
    }

    return location;
}

static void bst_insert(jfs_bst_t *tree, jfs_bst_node_t *node, const bst_location_t *location) {
    assert(location->node == tree->nil); // we don't handle dupes here

    node->left = tree->nil;
    node->right = tree->nil;
    bst_set_parent_color(node, location->parent, BST_RED);

    if (location->parent == tree->nil) {
        assert(tree->root == tree->nil);
        tree->root = node;
    } else {
        assert(location->parent_cmp != 0);
        if (location->parent_cmp == -1) {
            location->parent->left = node;
        } else {
            location->parent->right = node;
        }
    }

    bst_fixup_insert(tree, node);
}

static void bst_delete(jfs_bst_t *tree, jfs_bst_node_t *node) {
    assert(node != tree->nil); // must have something to delete

    jfs_bst_node_t *replacement = tree->nil;
    uint64_t        deleted_color = bst_color(node);

    if (node->left == tree->nil) {
        replacement = node->right;
        bst_transplant(tree, node, node->right);
    } else if (node->right == tree->nil) {
        replacement = node->left;
        bst_transplant(tree, node, node->left);
    } else {
        jfs_bst_node_t *next_largest = bst_local_minimum(tree, node->right);
        deleted_color = bst_color(next_largest);
        replacement = next_largest->right;

        if (next_largest != node->right) {
            bst_transplant(tree, next_largest, next_largest->right);
            next_largest->right = node->right;
            bst_set_parent(next_largest->right, next_largest);
        } else {
            bst_set_parent(replacement, next_largest);
        }

        bst_transplant(tree, node, next_largest);
        next_largest->left = node->left;
        bst_set_parent(next_largest->left, next_largest);
        bst_set_color(next_largest, bst_color(node));
    }

    if (deleted_color == BST_BLACK) {
        bst_fixup_delete(tree, replacement);
    }
}

static void bst_detach_and_delete(jfs_bst_t *tree, void *value_out, jfs_bst_node_t *node) {
    assert(tree != NULL);
    assert(tree->root != tree->nil); // tree can't be empty

    void *const       node_value = bst_container_value(tree, node);
    const void *const detach_value = tree->detach(node_value);
    memcpy(value_out, detach_value, tree->value_size); // value_out has been loaded

    if (detach_value == node_value) { // the detach is detaching the node linked on the tree
        bst_delete(tree, node);
        bst_update_cache_delete(tree, node);
        jfs_fl_free(&tree->free_list, node);
    } else { // the detach got a node that wasn't linked on the tree
        jfs_bst_node_t *const detach_node = bst_container_node(tree, detach_value);
        jfs_fl_free(&tree->free_list, detach_node);
    }
}

static void bst_update_cache_insert(jfs_bst_t *tree, const bst_location_t *location) {
    // we don't use location->node because this is assumed to be nil
    if (location->parent_cmp == -1 && location->parent == tree->cache.smallest) {
        tree->cache.smallest = location->parent->left;
    } else if (location->parent_cmp == 1 && location->parent == tree->cache.largest) {
        tree->cache.largest = location->parent->right;
    } else if (tree->cache.largest == tree->nil) { // we can assume if largest is nil so is smallest
        tree->cache.largest = tree->root;
        tree->cache.smallest = tree->root;
    }
}

static void bst_update_cache_delete(jfs_bst_t *tree, const jfs_bst_node_t *node) {
    if (tree->cache.smallest == tree->cache.largest) { // only one node in tree
        assert(node == tree->root);
        tree->cache.smallest = tree->nil;
        tree->cache.largest = tree->nil;
    } else if (node == tree->cache.smallest) {
        assert(tree->cache.smallest->left == tree->nil);

        if (tree->cache.smallest->right != tree->nil) {
            tree->cache.smallest = bst_local_minimum(tree, tree->cache.smallest->right);
        } else {
            tree->cache.smallest = bst_parent(tree->cache.smallest);
        }
    } else if (node == tree->cache.largest) {
        assert(tree->cache.largest->right == tree->nil);

        if (tree->cache.largest->left != tree->nil) {
            tree->cache.largest = bst_local_maximum(tree, tree->cache.largest->left);
        } else {
            tree->cache.largest = bst_parent(tree->cache.largest);
        }
    }
}

static void bst_fixup_insert(jfs_bst_t *tree, jfs_bst_node_t *node) {
    assert(node != tree->nil);
    assert(tree->root != tree->nil);    // after an insert the tree should never be empty
    assert(bst_color(node) == BST_RED); // all new nodes should be red

    while (bst_color(bst_parent(node)) == BST_RED) {
        jfs_bst_node_t       *parent = bst_parent(node);
        jfs_bst_node_t *const grandparent = bst_parent(parent);
        assert(grandparent != tree->nil);

        if (parent == grandparent->left) {
            jfs_bst_node_t *const uncle = grandparent->right;

            if (bst_color(uncle) == BST_RED) {
                bst_set_color(parent, BST_BLACK);
                bst_set_color(uncle, BST_BLACK);
                bst_set_color(grandparent, BST_RED);
                node = grandparent;
                // loop continues
            } else {
                if (node == parent->right) {
                    node = parent;
                    bst_rotate_left(tree, parent);
                    parent = bst_parent(node);
                }

                bst_set_color(parent, BST_BLACK);
                bst_set_color(grandparent, BST_RED);
                bst_rotate_right(tree, grandparent);
                break; // tree is fixed
            }
        } else {
            jfs_bst_node_t *const uncle = grandparent->left;

            if (bst_color(uncle) == BST_RED) {
                bst_set_color(parent, BST_BLACK);
                bst_set_color(uncle, BST_BLACK);
                bst_set_color(grandparent, BST_RED);
                node = grandparent;
                // loop continues
            } else {
                if (node == parent->left) {
                    node = parent;
                    bst_rotate_right(tree, parent);
                    parent = bst_parent(node);
                }

                bst_set_color(parent, BST_BLACK);
                bst_set_color(grandparent, BST_RED);
                bst_rotate_left(tree, grandparent);
                break; // tree is fixed
            }
        }
    }

    bst_set_color(tree->root, BST_BLACK);
}

static void bst_fixup_delete(jfs_bst_t *tree, jfs_bst_node_t *node) {
    while (node != tree->root && bst_color(node) == BST_BLACK) {
        jfs_bst_node_t *const parent = bst_parent(node);
        if (node == parent->left) {
            jfs_bst_node_t *sibling = parent->right;
            if (bst_color(sibling) == BST_RED) {
                bst_set_color(sibling, BST_BLACK);
                bst_set_color(parent, BST_RED);
                bst_rotate_left(tree, parent);
                sibling = parent->right;
            }

            if (bst_color(sibling->left) == BST_BLACK && bst_color(sibling->right) == BST_BLACK) {
                bst_set_color(sibling, BST_RED);
                node = parent;
            } else {
                if (bst_color(sibling->right) == BST_BLACK) {
                    bst_set_color(sibling->left, BST_BLACK);
                    bst_set_color(sibling, BST_RED);
                    bst_rotate_right(tree, sibling);
                    sibling = parent->right;
                }

                bst_set_color(sibling, bst_color(parent));
                bst_set_color(parent, BST_BLACK);
                bst_set_color(sibling->right, BST_BLACK);
                bst_rotate_left(tree, parent);
                node = tree->root;
            }
        } else {
            jfs_bst_node_t *sibling = parent->left;
            if (bst_color(sibling) == BST_RED) {
                bst_set_color(sibling, BST_BLACK);
                bst_set_color(parent, BST_RED);
                bst_rotate_right(tree, parent);
                sibling = parent->left;
            }

            if (bst_color(sibling->left) == BST_BLACK && bst_color(sibling->right) == BST_BLACK) {
                bst_set_color(sibling, BST_RED);
                node = parent;
            } else {
                if (bst_color(sibling->left) == BST_BLACK) {
                    bst_set_color(sibling->right, BST_BLACK);
                    bst_set_color(sibling, BST_RED);
                    bst_rotate_left(tree, sibling);
                    sibling = parent->left;
                }

                bst_set_color(sibling, bst_color(parent));
                bst_set_color(parent, BST_BLACK);
                bst_set_color(sibling->left, BST_BLACK);
                bst_rotate_right(tree, parent);
                node = tree->root;
            }
        }
    }

    bst_set_color(node, BST_BLACK);
}

static void bst_rotate_left(jfs_bst_t *tree, jfs_bst_node_t *x) {
    assert(x != tree->nil);
    assert(x->right != tree->nil);

    jfs_bst_node_t *const y = x->right;
    jfs_bst_node_t *const x_parent = bst_parent(x);

    x->right = y->left;
    if (y->left != tree->nil) bst_set_parent(y->left, x);

    bst_set_parent(y, x_parent);
    if (x_parent == tree->nil) {
        tree->root = y;
    } else if (x == x_parent->left) {
        x_parent->left = y;
    } else {
        x_parent->right = y;
    }

    y->left = x;
    bst_set_parent(x, y);
}

static void bst_rotate_right(jfs_bst_t *tree, jfs_bst_node_t *x) {
    assert(x != tree->nil);
    assert(x->left != tree->nil);

    jfs_bst_node_t *const y = x->left;
    jfs_bst_node_t *const x_parent = bst_parent(x);

    x->left = y->right;
    if (y->right != tree->nil) bst_set_parent(y->right, x);

    bst_set_parent(y, x_parent);
    if (x_parent == tree->nil) {
        tree->root = y;
    } else if (x == x_parent->left) {
        x_parent->left = y;
    } else {
        x_parent->right = y;
    }

    y->right = x;
    bst_set_parent(x, y);
}

static void bst_transplant(jfs_bst_t *tree, jfs_bst_node_t *old, jfs_bst_node_t *new) { // NOLINT
    assert(tree->root != tree->nil);
    assert(old != new);

    jfs_bst_node_t *const old_parent = bst_parent(old);

    if (old_parent == tree->nil) {
        tree->root = new;
    } else if (old_parent->left == old) {
        old_parent->left = new;
    } else {
        old_parent->right = new;
    }

    bst_set_parent(new, old_parent);
}

static jfs_bst_node_t *bst_local_minimum(const jfs_bst_t *tree, jfs_bst_node_t *start) {
    while (start->left != tree->nil) {
        start = start->left;
    }
    return start;
}

static jfs_bst_node_t *bst_local_maximum(const jfs_bst_t *tree, jfs_bst_node_t *start) {
    while (start->right != tree->nil) {
        start = start->right;
    }
    return start;
}

static jfs_bst_node_t *bst_parent(const jfs_bst_node_t *node) {
    assert(node != NULL);
    return (jfs_bst_node_t *) (node->parent_color & ~((uintptr_t) 1)); // NOLINT
}

static void bst_set_parent(jfs_bst_node_t *node, jfs_bst_node_t *parent) {
    assert(node != NULL);
    assert(parent != NULL);
    node->parent_color = (uintptr_t) parent | (node->parent_color & 1);
}

static uint64_t bst_color(const jfs_bst_node_t *node) {
    assert(node != NULL);
    return (uint64_t) (node->parent_color & 1);
}

static void bst_set_color(jfs_bst_node_t *node, uint64_t color) {
    assert(node != NULL);
    assert(color == BST_RED || color == BST_BLACK);
    node->parent_color = (node->parent_color & ~((uintptr_t) 1)) | color;
}

static void bst_set_parent_color(jfs_bst_node_t *node, jfs_bst_node_t *parent, uint64_t color) {
    assert(node != NULL);
    assert(parent != NULL);
    assert(color == BST_RED || color == BST_BLACK);
    node->parent_color = (uintptr_t) parent | color;
}
