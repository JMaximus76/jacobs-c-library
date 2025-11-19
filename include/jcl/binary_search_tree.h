#ifndef JFS_BINARY_SEARCH_TREE_H
#define JFS_BINARY_SEARCH_TREE_H

#include "free_list.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct jfs_bst       jfs_bst_t;
typedef struct jfs_bst_node  jfs_bst_node_t;
typedef struct jfs_bst_fns   jfs_bst_fns_t;
typedef struct jfs_bst_conf  jfs_bst_conf_t;
typedef struct jfs_bst_cache jfs_bst_cache_t;

typedef int (*jfs_bst_cmp_fn)(const void *key, const void *value);

struct jfs_bst_node {
    uintptr_t       parent_color;
    jfs_bst_node_t *right;
    jfs_bst_node_t *left;
    jfs_bst_node_t *list;
};

struct jfs_bst_conf {
    jfs_mlg_component_t *component;
    jfs_bst_cmp_fn       cmp;
};

struct jfs_bst_cache {
    jfs_bst_node_t *smallest;
    jfs_bst_node_t *largest;
    jfs_bst_node_t *previous;
};

struct jfs_bst {
    jfs_bst_node_t *root;
    jfs_bst_node_t *nil;
    jfs_bst_node_t  nil_storage;
    jfs_bst_cache_t cache;
    jfs_bst_cmp_fn  cmp;
    uintptr_t       value_offset;
    size_t          value_size;
    jfs_fl_t        free_list;
};

jfs_mlg_desc_t jfs_bst_make_desc(size_t obj_size, size_t obj_align, size_t obj_count, jfs_err_t *err);
void           jfs_bst_init(jfs_bst_t *tree_init, const jfs_bst_conf_t *conf, jfs_err_t *err);
void           jfs_bst_puts(jfs_bst_t *tree, const void *value, const void *key, jfs_err_t *err);
void           jfs_bst_takes(jfs_bst_t *tree, void *value_out, const void *key, jfs_err_t *err);
void           jfs_bst_get_largest(jfs_bst_t *tree, void *value_out, jfs_err_t *err);
void           jfs_bst_get_smallest(jfs_bst_t *tree, void *value_out, jfs_err_t *err);

#endif
