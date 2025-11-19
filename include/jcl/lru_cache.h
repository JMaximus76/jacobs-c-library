#ifndef JFS_LRU_CACHE_H
#define JFS_LRU_CACHE_H

#include "error.h"
#include "memory_block.h"
#include "memory_layout_generator.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct jfs_lru_conf jfs_lru_conf_t;
typedef struct jfs_lru_fn   jfs_lru_fn_t;
typedef struct jfs_lru      jfs_lru_t;
typedef int (*jfs_lru_cmp_fn)(const void *key, void *slot);
typedef void (*jfs_lru_slot_fn)(void *slot, void *ctx);

struct jfs_lru_fn {
    jfs_lru_cmp_fn  cmp;
    jfs_lru_slot_fn hit;
    jfs_lru_slot_fn miss;
    jfs_lru_slot_fn evict;
};

struct jfs_lru_conf {
    jfs_mlg_component_t *component;
    jfs_lru_fn_t         fn;
    void                *evict_ctx; // can null
};

struct jfs_lru {
    jfs_mb_t     mb;
    size_t       count;
    jfs_lru_fn_t fn;
    void        *evict_ctx;
};

jfs_mlg_desc_t jfs_lru_make_desc(size_t obj_size, size_t obj_align, size_t obj_count, jfs_err_t *err);
void jfs_lru_init(jfs_lru_t *lru_init, const jfs_lru_conf_t *conf, jfs_err_t *err);
void jfs_lru_free(jfs_lru_t *lru_move);
void jfs_lru_access(jfs_lru_t *lru, const void *key, void *user_ctx);

#endif
