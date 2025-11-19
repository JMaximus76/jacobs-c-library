#ifndef JFS_MEMORY_LAYOUT_GENERATOR_H
#define JFS_MEMORY_LAYOUT_GENERATOR_H

#include "error.h"
#include <stddef.h>
#include <stdint.h>

typedef struct jfs_mlg_desc      jfs_mlg_desc_t;
typedef struct jfs_mlg_layout    jfs_mlg_layout_t;
typedef struct jfs_mlg_component jfs_mlg_component_t;
typedef struct jfs_mlg_memory    jfs_mlg_memory_t;

struct jfs_mlg_desc {
    size_t size;
    size_t align;
    size_t count;
};

struct jfs_mlg_layout {
    jfs_mlg_desc_t *descriptions;
    size_t          descriptions_count;
    jfs_mlg_desc_t  header_desc;
};

struct jfs_mlg_component {
    void          *ptr;
    jfs_mlg_desc_t desc;
};

struct jfs_mlg_memory {
    jfs_mlg_component_t *component_list;
    size_t               component_count;
    void                *header;
};

inline int jfs_mlg_valid_align(size_t align);
int        jfs_mlg_valid_desc(const jfs_mlg_desc_t *desc);
int        jfs_mlg_valid_component(const jfs_mlg_component_t *component);
void      *jfs_mlg_apply_offset(void *ptr, uintptr_t offset);
size_t     jfs_mlg_align_size(size_t size, size_t align);
uintptr_t  jfs_mlg_append(jfs_mlg_desc_t *base, const jfs_mlg_desc_t *to_append);

jfs_mlg_memory_t *jfs_mlg_memory_init(const jfs_mlg_layout_t *layout, jfs_err_t *err);
void              jfs_mlg_memory_free(jfs_mlg_memory_t *memory_move);
#endif
