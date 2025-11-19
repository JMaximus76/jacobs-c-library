#ifndef JFS_SLAB_ALLOCATOR_H
#define JFS_SLAB_ALLOCATOR_H

#include "error.h"
#include <pthread.h>
#include <stdbool.h>

typedef struct jfs_sa_obj              jfs_sa_obj_t;
typedef struct jfs_sa_allocator        jfs_sa_allocator_t;
typedef struct jfs_sa_allocator_config jfs_sa_allocator_config_t;
typedef struct jfs_sa_cache            jfs_sa_cache_t;

struct jfs_sa_allocator_config {
    size_t   obj_size;
    size_t   obj_align;
    uint32_t batch_capacity;       // zero for default
    uint32_t alloc_store_capacity; // zero for default
    uint32_t cache_store_capacity; // zero for default
    uint32_t cache_acquire_amount; // zero for default
    uint32_t cache_release_amount; // zero for default
    uint32_t slab_acquire_count;   // zero for default
};

jfs_sa_allocator_t *jfs_sa_allocator_create(const jfs_sa_allocator_config_t *config, jfs_err_t *err) WUR;
void                jfs_sa_allocator_destroy(jfs_sa_allocator_t *alloc_move); // MUST ENSURE that no threads can use the allocator when this is called

jfs_sa_cache_t *jfs_sa_cache_create(const jfs_sa_allocator_t *alloc, jfs_err_t *err) WUR;
void            jfs_sa_cache_destroy(jfs_sa_cache_t *cache_move);
void           *jfs_sa_alloc(jfs_sa_cache_t *cache, jfs_err_t *err) WUR;
void            jfs_sa_free(jfs_sa_cache_t *cache, void *free);

#endif
