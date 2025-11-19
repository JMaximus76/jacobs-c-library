#ifndef JFS_MEMORY_BLOCK_H
#define JFS_MEMORY_BLOCK_H

#include "error.h"
#include "memory_layout_generator.h"
#include <stddef.h>
#include <stdint.h>

typedef struct jfs_mb jfs_mb_t;

struct jfs_mb {
    uint8_t *base_ptr;
    size_t   obj_size;
    size_t   capacity;
};

void  jfs_mb_init(jfs_mb_t *mb_init, const jfs_mlg_component_t *component, jfs_err_t *err);
void *jfs_mb_index(const jfs_mb_t *mb, size_t index);
void  jfs_mb_write(jfs_mb_t *mb, const void *obj, size_t index);
void  jfs_mb_read(jfs_mb_t *mb, void *obj, size_t index);
void  jfs_mb_remap(jfs_mb_t *mb, size_t dest_index, size_t src_index, size_t obj_count);

#endif
