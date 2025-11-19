#include "lru_cache.h"
#include <assert.h>

static void lru_promote(jfs_lru_t *lru, size_t index);
static int lru_valid_fn(const jfs_lru_fn_t *fn);

jfs_mlg_desc_t jfs_lru_make_desc(size_t obj_size, size_t obj_align, size_t obj_count, jfs_err_t *err) {
    const jfs_mlg_desc_t desc = {
        .align = obj_align,
        .size = obj_size,
        .count = obj_count + 1 // extra slot for promote algorithm
    };

    VAL_FAIL_IF(!jfs_mlg_valid_desc(&desc), JFS_ERR_ARG, (jfs_mlg_desc_t){0});
    return desc;
}

void jfs_lru_init(jfs_lru_t *lru_init, const jfs_lru_conf_t *conf, jfs_err_t *err) {
    VOID_FAIL_IF(!lru_valid_fn(&conf->fn), JFS_ERR_BAD_CONF);

    jfs_mb_init(&lru_init->mb, conf->component, err);
    VOID_CHECK_ERR;

    lru_init->count = 0;
    lru_init->fn = conf->fn;
    lru_init->evict_ctx = conf->evict_ctx;
}

void jfs_lru_free(jfs_lru_t *lru_move) {
    for (size_t i = 0; i < lru_move->count; i++) {
        lru_move->fn.evict(jfs_mb_index(&lru_move->mb, i), lru_move->evict_ctx);
    }
}

void jfs_lru_access(jfs_lru_t *lru, const void *key, void *user_ctx) { // NOLINT
    for (size_t i = 0; i < lru->count; i++) {
        void *slot_ptr = jfs_mb_index(&lru->mb, i);
        if (lru->fn.cmp(key, slot_ptr) == 0) {
            lru->fn.hit(slot_ptr, user_ctx);
            if (i != 0) lru_promote(lru, i);
            return;
        }
    }

    void *slot = NULL;
    if (lru->count < lru->mb.capacity) {
        slot = jfs_mb_index(&lru->mb, lru->count);
        lru->count += 1;
    } else {
        assert(lru->count == lru->mb.capacity);
        slot = jfs_mb_index(&lru->mb, lru->count - 1);
        lru->fn.evict(slot, lru->evict_ctx);
    }

    lru->fn.miss(slot, user_ctx);
    lru_promote(lru, lru->count - 1);
}

static void lru_promote(jfs_lru_t *lru, size_t index) {
    uint8_t temp_slot[lru->mb.obj_size];
    jfs_mb_read(&lru->mb, temp_slot, index);
    jfs_mb_remap(&lru->mb, 1, 0, index);
    jfs_mb_write(&lru->mb, temp_slot, 0);
}

static int lru_valid_fn(const jfs_lru_fn_t *fn) {
    return fn->cmp != NULL && fn->evict != NULL && fn->hit != NULL && fn->miss != NULL;
}
