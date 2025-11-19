#include "free_list.h"
#include "error.h"
#include "memory_layout_generator.h"
#include <assert.h>
#include <stdlib.h>

#define MIN_OBJ_SIZE sizeof(jfs_fl_obj_t)

static jfs_fl_obj_t *fl_link_memory(uint8_t *memory, size_t padded_obj_size, size_t obj_count);

void jfs_fl_init(jfs_fl_t *fl_init, const jfs_mlg_component_t *component, jfs_err_t *err) {
    VOID_FAIL_IF(!jfs_mlg_valid_component(component), JFS_ERR_BAD_CONF);
    fl_init->list = fl_link_memory(component->ptr, component->desc.size, component->desc.count);
    fl_init->count = component->desc.count;
}

void *jfs_fl_alloc(jfs_fl_t *fl) {
    if (fl->count == 0) return NULL;
    jfs_fl_obj_t *obj = fl->list;
    fl->list = obj->next;
    fl->count -= 1;
    return obj;
}

void jfs_fl_free(jfs_fl_t *fl, void *ptr_move) {
    if (ptr_move == NULL) return;
    jfs_fl_obj_t *obj = ptr_move;
    obj->next = fl->list;
    fl->list = obj;
    fl->count += 1;
}

static jfs_fl_obj_t *fl_link_memory(uint8_t *memory, size_t obj_size, size_t obj_count) { // NOLINT
    jfs_fl_obj_t *list = NULL;

    for (size_t i = 0; i < obj_count; i++) {
        jfs_fl_obj_t *obj = (jfs_fl_obj_t *) (memory + (i * obj_size));
        obj->next = list;
        list = obj;
    }

    return list;
}
