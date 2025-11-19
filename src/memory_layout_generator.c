#include "memory_layout_generator.h"
#include "error.h"
#include <assert.h>
#include <stdalign.h>
#include <stdint.h>
#include <stdlib.h>

inline int jfs_mlg_valid_align(size_t align) {
    return align && ((align - 1) & align) == 0;
}

int jfs_mlg_valid_desc(const jfs_mlg_desc_t *desc) {
    return jfs_mlg_valid_align(desc->align) && desc->size > 0 && desc->count > 0;
}

int jfs_mlg_valid_component(const jfs_mlg_component_t *component) {
    return jfs_mlg_valid_desc(&component->desc) && component->ptr;
}

void *jfs_mlg_apply_offset(void *ptr, uintptr_t offset) {
    assert(offset > 0);
    return ((uint8_t *) ptr) + offset;
}

size_t jfs_mlg_align_size(size_t size, size_t align) {
    assert(jfs_mlg_valid_align(align));
    assert(size > 0);
    return (size + align - 1) & ~(align - 1);
}

uintptr_t jfs_mlg_append(jfs_mlg_desc_t *base, const jfs_mlg_desc_t *to_append) {
    assert(jfs_mlg_valid_desc(base));
    assert(jfs_mlg_valid_desc(to_append));

    const size_t aligned_base_size = jfs_mlg_align_size(base->size, to_append->align);
    base->size = aligned_base_size + (to_append->size * to_append->count);
    if (to_append->align > base->align) base->align = to_append->align;
    return aligned_base_size; // this is the offset
}

jfs_mlg_memory_t *jfs_mlg_memory_init(const jfs_mlg_layout_t *layout, jfs_err_t *err) {
    NULL_FAIL_IF(!jfs_mlg_valid_desc(&layout->header_desc), JFS_ERR_BAD_CONF);

    jfs_mlg_desc_t       memory_desc = {.size = sizeof(jfs_mlg_memory_t), .align = alignof(jfs_mlg_memory_t), .count = 1};
    const jfs_mlg_desc_t component_list_desc = {.size = sizeof(jfs_mlg_component_t),
                                                .align = alignof(jfs_mlg_component_t),
                                                .count = layout->descriptions_count};

    const uintptr_t component_list_offset = jfs_mlg_append(&memory_desc, &component_list_desc);
    const uintptr_t header_offset = jfs_mlg_append(&memory_desc, &layout->header_desc);
    uintptr_t      *component_offsets = jfs_malloc(sizeof(uintptr_t) * layout->descriptions_count, err);
    NULL_CHECK_ERR;

    for (size_t i = 0; i < layout->descriptions_count; i++) {
        const jfs_mlg_desc_t *const desc = &layout->descriptions[i];
        if (!jfs_mlg_valid_desc(desc)) GOTO_WITH_ERR(cleanup, JFS_ERR_BAD_CONF);
        component_offsets[i] = jfs_mlg_append(&memory_desc, desc);
    }

    jfs_mlg_memory_t *const memory = jfs_aligned_alloc(memory_desc.align, memory_desc.size, err);
    GOTO_IF_ERR(cleanup);

    memory->component_list = jfs_mlg_apply_offset(memory, component_list_offset);
    memory->header = jfs_mlg_apply_offset(memory, header_offset);
    memory->component_count = layout->descriptions_count;

    for (size_t i = 0; i < memory->component_count; i++) {
        memory->component_list[i].desc = layout->descriptions[i];
        memory->component_list[i].ptr = jfs_mlg_apply_offset(memory, component_offsets[i]);
    }

    free(component_offsets);
    return memory;
cleanup:
    free(component_offsets);
    return NULL;
}

void jfs_mlg_memory_free(jfs_mlg_memory_t *memory_move) {
    free(memory_move);
}
