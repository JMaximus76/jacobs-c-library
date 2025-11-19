#include "slab_allocator.h"
#include "binary_search_tree.h"
#include "error.h"
#include "free_list.h"
#include "lru_cache.h"
#include <assert.h>
#include <limits.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

// defualts
#define DEFAULT_CACHE_STORE_CAPACITY 2
#define DEFAULT_CACHE_ACQUIRE_AMOUNT 1
#define DEFAULT_CACHE_RELEASE_AMOUNT 1
#define DEFAULT_BATCH_CAPACITY       64
#define DEFAULT_ACQUIRE_PER_SLAB     8
#define DEFAULT_ALLOC_STORE_CAPACITY 4

// consts
#define MIN_OBJ_SIZE sizeof(jfs_sa_obj_t)
#define PAGE_SIZE    ((size_t) 4096) // 4 kb

typedef struct sa_batch  sa_batch_t;
typedef struct sa_buffer sa_buffer_t;
typedef struct sa_config sa_config_t;
typedef struct sa_slab   sa_slab_t;

struct jfs_sa_obj {
    jfs_sa_obj_t *next;
};

struct sa_batch {
    uint64_t slab_id;
    jfs_fl_t free_list;
};

struct sa_slab {
    uint64_t      id;
    atomic_size_t used_count;
};

struct sa_config {
    size_t    obj_align;
    size_t    obj_padded_size;
    size_t    slab_size;
    uintptr_t slab_offset;
    uintptr_t slab_obj_mask;
    uint32_t  batch_capacity;
    size_t    batch_per_slab;
    uint32_t  alloc_store_capacity;
    uint32_t  cache_store_capacity;
    uint32_t  cache_acquire_amount;
    uint32_t  cache_release_amount;
};

struct jfs_sa_allocator {
    const sa_config_t conf;
    pthread_mutex_t   lock;
    jfs_bst_t         store;
};

struct jfs_sa_cache {
    jfs_sa_allocator_t *alloc;
    sa_batch_t          active_batch;
    jfs_lru_t           returns_cache;
    jfs_bst_t           store;
};

static void  sa_alloc_slow_path(jfs_sa_cache_t *cache, jfs_err_t *err);
static void  sa_free_slow_path(jfs_sa_cache_t *cache);
static void  sa_config_init(sa_config_t *conf_init, const jfs_sa_allocator_config_t *alloc_conf, jfs_err_t *err);
static void  sa_print_config(const sa_config_t *config);
static void *sa_aligned_mmap(const sa_config_t *conf, jfs_err_t *err) WUR;

static sa_slab_t *sa_slab_create(const sa_config_t *conf, uint64_t new_slab_id, jfs_err_t *err) WUR;
static void       sa_slab_link_batches(const sa_config_t *conf, sa_slab_t *slab, sa_batch_t *batch_buffer);
static void       sa_slab_destroy(sa_slab_t *slab_move, const sa_config_t *conf);

static void sa_batch_init(sa_batch_t *batch_init);
static void sa_batch_transfer(sa_batch_t *init, sa_batch_t *free);
static bool sa_batch_pack(sa_batch_t *batch, jfs_sa_obj_t *obj_move);
static bool sa_batch_unpack(sa_batch_t *batch, jfs_sa_obj_t *obj_move);
static bool sa_batch_is_full(const sa_batch_t *batch) WUR;

static sa_buffer_t sa_buffer_create(const sa_config_t *conf, jfs_err_t *err) WUR;
static void        sa_buffer_destroy(sa_buffer_t *buf);
static bool        sa_buffer_enqueue(sa_buffer_t *buf, sa_batch_t *batch_free);
static bool        sa_buffer_dequeue(sa_buffer_t *buf, sa_batch_t *batch_init);
static bool        sa_buffer_pipe(sa_buffer_t *dest, sa_buffer_t *src, uint32_t count);

static void       sa_allocator_add_batches(jfs_sa_allocator_t *alloc, jfs_err_t *err);
static sa_slab_t *sa_allocator_find_slab(const jfs_sa_allocator_t *alloc, const jfs_sa_obj_t *obj) WUR;
static void       sa_allocator_release_obj(jfs_sa_allocator_t *alloc, jfs_sa_obj_t *obj_free);

jfs_sa_allocator_t *jfs_sa_allocator_create(const jfs_sa_allocator_config_t *config, jfs_err_t *err) WUR;
void                jfs_sa_allocator_destroy(jfs_sa_allocator_t *alloc_move);
jfs_sa_cache_t     *jfs_sa_cache_create(const jfs_sa_allocator_t *alloc, jfs_err_t *err) WUR;
void                jfs_sa_cache_destroy(jfs_sa_cache_t *cache_move);
void               *jfs_sa_alloc(jfs_sa_cache_t *cache, jfs_err_t *err) WUR;
void                jfs_sa_free(jfs_sa_cache_t *cache, void *free);

static void sa_alloc_slow_path(jfs_sa_cache_t *cache, jfs_err_t *err);
static void sa_free_slow_path(jfs_sa_cache_t *cache);

static void sa_config_init(sa_config_t *conf_init, const jfs_sa_allocator_config_t *alloc_conf, jfs_err_t *err) {
    conf_init->obj_align = alloc_conf->obj_align;
    VOID_FAIL_IF(conf_init->obj_align == 0, JFS_ERR_BAD_CONF);
    VOID_FAIL_IF(conf_init->obj_align & (conf_init->obj_align - 1), JFS_ERR_BAD_CONF);

    const size_t obj_size = alloc_conf->obj_size > MIN_OBJ_SIZE ? alloc_conf->obj_size : MIN_OBJ_SIZE;
    conf_init->obj_padded_size = (obj_size + conf_init->obj_align - 1) & ~(conf_init->obj_align - 1);
    assert(conf_init->obj_padded_size % conf_init->obj_align == 0);

    conf_init->slab_offset = (sizeof(sa_slab_t) + conf_init->obj_align - 1) & ~(conf_init->obj_align - 1);
    assert(conf_init->slab_offset % conf_init->obj_align == 0);

    conf_init->batch_capacity = alloc_conf->batch_capacity ? alloc_conf->batch_capacity : DEFAULT_BATCH_CAPACITY;
    conf_init->cache_acquire_amount = alloc_conf->cache_acquire_amount ? alloc_conf->cache_acquire_amount : DEFAULT_CACHE_ACQUIRE_AMOUNT;
    conf_init->cache_release_amount = alloc_conf->cache_release_amount ? alloc_conf->cache_release_amount : DEFAULT_CACHE_RELEASE_AMOUNT;
    conf_init->cache_store_capacity = alloc_conf->cache_store_capacity ? alloc_conf->alloc_store_capacity : DEFAULT_CACHE_STORE_CAPACITY;
    conf_init->alloc_store_capacity = alloc_conf->alloc_store_capacity ? alloc_conf->alloc_store_capacity : DEFAULT_ALLOC_STORE_CAPACITY;
    VOID_FAIL_IF(conf_init->cache_acquire_amount > conf_init->cache_store_capacity, JFS_ERR_BAD_CONF);
    VOID_FAIL_IF(conf_init->cache_release_amount > conf_init->cache_store_capacity, JFS_ERR_BAD_CONF);
    VOID_FAIL_IF(conf_init->cache_acquire_amount > conf_init->alloc_store_capacity, JFS_ERR_BAD_CONF);
    VOID_FAIL_IF(conf_init->cache_release_amount > conf_init->alloc_store_capacity, JFS_ERR_BAD_CONF);

    const size_t acquire_per_slab = alloc_conf->slab_acquire_count ? alloc_conf->slab_acquire_count : DEFAULT_ACQUIRE_PER_SLAB;
    const size_t slab_size_needed =
        (acquire_per_slab * conf_init->cache_acquire_amount * conf_init->batch_capacity * conf_init->obj_padded_size) + conf_init->slab_offset;
    const size_t min_pages_needed = (slab_size_needed + PAGE_SIZE - 1) / PAGE_SIZE;
    VOID_FAIL_IF(min_pages_needed > ULONG_MAX / 2, JFS_ERR_BAD_CONF); // insane amounts of memory right here
    size_t pages_per_slab = 1;
    while (pages_per_slab < min_pages_needed) {
        pages_per_slab *= 2;
    }

    conf_init->slab_size = pages_per_slab * PAGE_SIZE;
    VOID_FAIL_IF(conf_init->slab_size >= slab_size_needed, JFS_ERR_BAD_CONF);
    assert(conf_init->slab_size & (conf_init->slab_size - 1));

    conf_init->slab_obj_mask = ~(conf_init->slab_size - 1);

    const size_t batch_total_bytes = (conf_init->batch_capacity * conf_init->obj_padded_size);
    conf_init->batch_per_slab = (conf_init->slab_size - conf_init->slab_offset + batch_total_bytes - 1) / batch_total_bytes;
    assert(acquire_per_slab * conf_init->cache_acquire_amount <= conf_init->batch_per_slab);
    VOID_FAIL_IF(conf_init->batch_per_slab > conf_init->alloc_store_capacity, JFS_ERR_BAD_CONF);
}

static void sa_print_config(const sa_config_t *config) { }

static void *sa_aligned_mmap(const sa_config_t *conf, jfs_err_t *err) { // NOLINT(readability-function-cognitive-complexity)
    assert(conf->slab_size > 0);
    assert(err != NULL);

    void *const raw_block = jfs_mmap(NULL, conf->slab_size * 2, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0, err);
    NULL_CHECK_ERR;

    const uintptr_t raw_addr = (uintptr_t) raw_block;
    const uintptr_t aligned_addr = (raw_addr + conf->slab_size - 1) & ~(conf->slab_size - 1);
    void *const     aligned_block = (void *) aligned_addr; // NOLINT
    const size_t    leading_trim = aligned_addr - raw_addr;
    const size_t    trailing_trim = (raw_addr + 2 * conf->slab_size) - (aligned_addr + conf->slab_size);

    assert(raw_addr % PAGE_SIZE == 0);
    assert(aligned_addr % conf->slab_size == 0);
    assert(leading_trim % PAGE_SIZE == 0);
    assert(trailing_trim % PAGE_SIZE == 0);

    // these munmap are assuming:
    //   - both addresses are within the proceses address space
    //   - len is > 0 (the ifs check this)
    //   - the addr's are multiples of page_size which the asserts above check
    if (leading_trim > 0) {
        int ret = munmap(raw_block, leading_trim); // NOLINT
        assert(ret == 0 && "munmap shouldn't be able to fail here");
    }

    if (trailing_trim > 0) {
        int ret = munmap((void *) aligned_addr + conf->slab_size, trailing_trim); // NOLINT
        assert(ret == 0 && "munmap shouldn't be able to fail here");
    }

    return aligned_block;
}

static sa_slab_t *sa_allocator_find_slab(const jfs_sa_allocator_t *alloc, const jfs_sa_obj_t *obj) {
    return (sa_slab_t *) (((uintptr_t) obj) & ~(alloc->conf.slab_size - 1)); // NOLINT
}

static sa_slab_t *sa_slab_create(const sa_config_t *conf, uint64_t new_slab_id, jfs_err_t *err) {
    sa_slab_t *slab = sa_aligned_mmap(conf, err);
    NULL_CHECK_ERR;
    slab->id = new_slab_id;
    slab->used_count = 0;
    return slab;
}

static void sa_slab_link_batches(const sa_config_t *conf, sa_slab_t *slab, sa_batch_t *batch_buffer) {
    sa_batch_t batch = {0};

    uintptr_t  slab_ptr_index = (uintptr_t) slab + conf->slab_offset;
    for (size_t i = 0; i < conf->batch_per_slab; i++) {
        for (uint32_t i = 0; i < conf->batch_capacity; i++) {
            
        }
    }
}

static void sa_slab_destroy(sa_slab_t *slab_move, const sa_config_t *conf);
