#include "memory_block.h"
#include "error.h"
#include <assert.h>

void jfs_mb_init(jfs_mb_t *mb_init, const jfs_mlg_component_t *component, jfs_err_t *err) {
    VOID_FAIL_IF(!jfs_mlg_valid_component(component), JFS_ERR_BAD_CONF);

    mb_init->base_ptr = component->ptr;
    mb_init->obj_size = component->desc.size;
    mb_init->capacity = component->desc.count;
}

void *jfs_mb_index(const jfs_mb_t *mb, size_t index) {
    return mb->base_ptr + (index * mb->obj_size);
}

void jfs_mb_write(jfs_mb_t *mb, const void *obj, size_t index) {
    memcpy(jfs_mb_index(mb, index), obj, mb->obj_size);
}

void jfs_mb_read(jfs_mb_t *mb, void *obj, size_t index) {
    memcpy(obj, jfs_mb_index(mb, index), mb->obj_size);
}

void jfs_mb_remap(jfs_mb_t *mb, size_t dest_index, size_t src_index, size_t obj_count) {
    memmove(jfs_mb_index(mb, dest_index), jfs_mb_index(mb, src_index), mb->obj_size * obj_count);
}

