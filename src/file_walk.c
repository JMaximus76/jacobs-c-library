#include "file_walk.h"
#include "error.h"
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#define FW_PATH_VECTOR_DEFAULT_CAPACITY 16
#define FW_FILE_VECTOR_DEFAULT_CAPACITY 16
#define FW_DIR_VECTOR_DEFAULT_CAPACITY  16

typedef struct fw_path_vector fw_path_vector_t;
typedef struct fw_file_vector fw_file_vector_t;
typedef struct fw_dir_vector  fw_dir_vector_t;

struct fw_path_vector {
    size_t          count;
    size_t          capacity;
    jfs_fio_path_t *path_array;
};

struct fw_file_vector {
    size_t         count;
    size_t         capacity;
    jfs_fw_file_t *file_array;
};

struct fw_dir_vector {
    size_t        count;
    size_t        capacity;
    jfs_fw_dir_t *dir_array;
};

struct jfs_fw_state {
    fw_path_vector_t path_vec;
    fw_dir_vector_t  dir_vec;
};

static jfs_fw_types_t fw_map_dirent_type(unsigned char ent_type, jfs_err_t *err);
static void           fw_file_init(jfs_fw_file_t *file_init, const struct dirent *ent, jfs_err_t *err);
static void           fw_dir_init(jfs_fw_dir_t *dir_init, fw_file_vector_t *vec_free, jfs_fio_path_t *path_free, jfs_err_t *err);

static void fw_path_vector_init(fw_path_vector_t *vec_init, jfs_err_t *err);
static void fw_path_vector_free(fw_path_vector_t *vec_free);
static void fw_path_vector_push(fw_path_vector_t *vec, jfs_fio_path_t *path_free, jfs_err_t *err);
static void fw_path_vector_pop(fw_path_vector_t *vec, jfs_fio_path_t *path_init, jfs_err_t *err);

static void           fw_file_vector_init(fw_file_vector_t *vec_init, jfs_err_t *err);
static void           fw_file_vector_free(fw_file_vector_t *vec_free);
static void           fw_file_vector_push(fw_file_vector_t *vec, jfs_fw_file_t *file_free, jfs_err_t *err);
static jfs_fw_file_t *fw_file_vector_to_array(fw_file_vector_t *vec_free, jfs_err_t *err) WUR;

static void          fw_dir_vector_init(fw_dir_vector_t *vec_init, jfs_err_t *err);
static void          fw_dir_vector_free(fw_dir_vector_t *vec_free);
static void          fw_dir_vector_push(fw_dir_vector_t *vec, jfs_fw_dir_t *dir_free, jfs_err_t *err);
static jfs_fw_dir_t *fw_dir_vector_to_array(fw_dir_vector_t *vec_free, jfs_err_t *err) WUR;

static void fw_scan_dir(DIR *dir, fw_file_vector_t *vec, jfs_err_t *err);
static void fw_handle_dirent(const struct dirent *ent, fw_file_vector_t *vec, jfs_err_t *err);
static void fw_push_dir_paths(fw_path_vector_t *path_vec, fw_file_vector_t *file_vec, const jfs_fio_path_t *dir_path, jfs_err_t *err);

void jfs_fw_file_free(jfs_fw_file_t *file_free) {
    jfs_fio_name_free(&file_free->name);
    memset(file_free, 0, sizeof(*file_free));
}

void jfs_fw_file_transfer(jfs_fw_file_t *file_init, jfs_fw_file_t *file_free) {
    *file_init = *file_free;
    memset(file_free, 0, sizeof(*file_free));
}

void jfs_fw_dir_free(jfs_fw_dir_t *dir_free) {
    if (dir_free->files != NULL) {
        for (size_t i = 0; i < dir_free->file_count; i++) {
            jfs_fw_file_free(&dir_free->files[i]);
        }

        free(dir_free->files);
    }

    jfs_fio_path_free(&dir_free->path);

    memset(dir_free, 0, sizeof(*dir_free));
}

void jfs_fw_dir_transfer(jfs_fw_dir_t *dir_init, jfs_fw_dir_t *dir_free) {
    *dir_init = *dir_free;
    memset(dir_free, 0, sizeof(*dir_free));
}

jfs_fw_state_t *jfs_fw_state_create(const jfs_fio_path_t *start_path, jfs_err_t *err) {
    jfs_fw_state_t *state = NULL;
    jfs_fio_path_t  new_path = {0};

    state = jfs_malloc(sizeof(*state), err);
    GOTO_IF_ERR(cleanup);

    fw_path_vector_init(&state->path_vec, err);
    GOTO_IF_ERR(cleanup);

    fw_dir_vector_init(&state->dir_vec, err);
    GOTO_IF_ERR(cleanup);

    jfs_fio_path_init(&new_path, start_path->str, err);
    GOTO_IF_ERR(cleanup);

    fw_path_vector_push(&state->path_vec, &new_path, err);
    GOTO_IF_ERR(cleanup);

    return state;
cleanup:
    if (state != NULL) {
        fw_dir_vector_free(&state->dir_vec);
        fw_path_vector_free(&state->path_vec);
        free(state);
    }

    jfs_fio_path_free(&new_path);
    NULL_RETURN_ERR;
}

void jfs_fw_state_destroy(jfs_fw_state_t *state_move) {
    if (state_move == NULL) return;

    fw_path_vector_free(&state_move->path_vec);
    fw_dir_vector_free(&state_move->dir_vec);

    free(state_move);
}

int jfs_fw_state_step(jfs_fw_state_t *state, jfs_err_t *err) { // NOLINT(readability-function-cognitive-complexity)
    if (state->path_vec.count == 0) return 1;

    fw_file_vector_t file_vec = {0};
    jfs_fio_path_t   dir_path = {0};
    jfs_fw_dir_t     dir = {0};
    DIR             *sys_dir = NULL;

    fw_file_vector_init(&file_vec, err);
    GOTO_IF_ERR(cleanup);

    fw_path_vector_pop(&state->path_vec, &dir_path, err);
    GOTO_IF_ERR(cleanup);

    sys_dir = jfs_opendir(dir_path.str, err);
    GOTO_IF_ERR(cleanup);

    fw_scan_dir(sys_dir, &file_vec, err);
    GOTO_IF_ERR(cleanup);

    fw_push_dir_paths(&state->path_vec, &file_vec, &dir_path, err);
    GOTO_IF_ERR(cleanup);

    fw_dir_init(&dir, &file_vec, &dir_path, err);
    GOTO_IF_ERR(cleanup);

    fw_dir_vector_push(&state->dir_vec, &dir, err);
    GOTO_IF_ERR(cleanup);

    closedir(sys_dir);
    return state->path_vec.count > 0;

cleanup:
    if (sys_dir != NULL) closedir(sys_dir);
    fw_file_vector_free(&file_vec);
    jfs_fio_path_free(&dir_path);
    jfs_fw_dir_free(&dir);
    REMAP_ERR(JFS_ERR_ACCESS, JFS_ERR_FW_SKIP);
    REMAP_ERR(JFS_ERR_INVAL_PATH, JFS_ERR_FW_FAIL);
    VAL_RETURN_ERR(state->path_vec.count == 0);
}

void jfs_fw_record_init(jfs_fw_record_t *record_init, jfs_fw_state_t *state_move, jfs_err_t *err) {
    VOID_FAIL_IF(state_move->path_vec.count > 0, JFS_ERR_FW_STATE);

    size_t        new_dir_count = state_move->dir_vec.count;
    jfs_fw_dir_t *new_dir_array = fw_dir_vector_to_array(&state_move->dir_vec, err);
    VOID_CHECK_ERR;

    record_init->dir_array = new_dir_array;
    record_init->dir_count = new_dir_count;

    jfs_fw_state_destroy(state_move);
}

void jfs_fw_record_free(jfs_fw_record_t *record_free) {
    if (record_free->dir_array != NULL) {
        for (size_t i = 0; i < record_free->dir_count; i++) {
            jfs_fw_dir_free(&record_free->dir_array[i]);
        }

        free(record_free->dir_array);
    }

    memset(record_free, 0, sizeof(*record_free));
}

static jfs_fw_types_t fw_map_dirent_type(unsigned char ent_type, jfs_err_t *err) {
    switch (ent_type) {
        case DT_REG:     return JFS_FW_REG;
        case DT_DIR:     return JFS_FW_DIR;
        case DT_UNKNOWN: *err = JFS_ERR_FW_UNKNOWN; VAL_RETURN_ERR(0);
        default:         *err = JFS_ERR_FW_UNSUPPORTED; VAL_RETURN_ERR(0);
    }
}

static void fw_file_init(jfs_fw_file_t *file_init, const struct dirent *ent, jfs_err_t *err) {
    jfs_fw_types_t new_type = 0;
    jfs_fio_name_t new_name = {0};

    new_type = fw_map_dirent_type(ent->d_type, err);
    VOID_CHECK_ERR;
    jfs_fio_name_init(&new_name, ent->d_name, err);
    VOID_CHECK_ERR;

    jfs_fio_name_transfer(&file_init->name, &new_name);
    file_init->inode = ent->d_ino;
    file_init->type = new_type;
}

static void fw_dir_init(jfs_fw_dir_t *dir_init, fw_file_vector_t *vec_free, jfs_fio_path_t *path_free, jfs_err_t *err) {
    size_t         new_count = vec_free->count;
    jfs_fw_file_t *new_files = NULL;

    if (vec_free->count > 0) {
        new_files = fw_file_vector_to_array(vec_free, err);
        VOID_CHECK_ERR;
    } else {
        fw_file_vector_free(vec_free);
    }

    dir_init->file_count = new_count;
    dir_init->files = new_files;
    jfs_fio_path_transfer(&dir_init->path, path_free);
}

static void fw_path_vector_init(fw_path_vector_t *vec_init, jfs_err_t *err) {
    jfs_fio_path_t *new_path_array = jfs_malloc(sizeof(*new_path_array) * FW_PATH_VECTOR_DEFAULT_CAPACITY, err);
    VOID_CHECK_ERR;

    vec_init->path_array = new_path_array;
    vec_init->capacity = FW_PATH_VECTOR_DEFAULT_CAPACITY;
    vec_init->count = 0;
}

static void fw_path_vector_free(fw_path_vector_t *vec_free) {
    if (vec_free->path_array != NULL) {
        for (size_t i = 0; i < vec_free->count; i++) {
            jfs_fio_path_free(&vec_free->path_array[i]);
        }

        free(vec_free->path_array);
    }

    memset(vec_free, 0, sizeof(*vec_free));
}

static void fw_path_vector_push(fw_path_vector_t *vec, jfs_fio_path_t *path_free, jfs_err_t *err) {
    if (vec->count >= vec->capacity) {
        const size_t new_capacity = vec->capacity * 2;

        jfs_fio_path_t *new_path_array = jfs_realloc(vec->path_array, sizeof(*new_path_array) * new_capacity, err);
        VOID_CHECK_ERR;

        vec->path_array = new_path_array;
        vec->capacity = new_capacity;
    }

    jfs_fio_path_transfer(&vec->path_array[vec->count], path_free);
    vec->count += 1;
}

static void fw_path_vector_pop(fw_path_vector_t *vec, jfs_fio_path_t *path_init, jfs_err_t *err) {
    VOID_FAIL_IF(vec->count == 0, JFS_ERR_EMPTY);

    vec->count -= 1;
    jfs_fio_path_transfer(path_init, &vec->path_array[vec->count]);
}

static void fw_file_vector_init(fw_file_vector_t *vec_init, jfs_err_t *err) {
    jfs_fw_file_t *new_file_array = jfs_malloc(sizeof(*new_file_array) * FW_FILE_VECTOR_DEFAULT_CAPACITY, err);
    VOID_CHECK_ERR;

    vec_init->file_array = new_file_array;
    vec_init->capacity = FW_FILE_VECTOR_DEFAULT_CAPACITY;
    vec_init->count = 0;
}

static void fw_file_vector_free(fw_file_vector_t *vec_free) {
    if (vec_free->file_array != NULL) {
        for (size_t i = 0; i < vec_free->count; i++) {
            jfs_fw_file_free(&vec_free->file_array[i]);
        }

        free(vec_free->file_array);
    }

    memset(vec_free, 0, sizeof(*vec_free));
}

static void fw_file_vector_push(fw_file_vector_t *vec, jfs_fw_file_t *file_free, jfs_err_t *err) {
    if (vec->count >= vec->capacity) {
        const size_t new_capacity = vec->capacity * 2;

        jfs_fw_file_t *new_file_array = jfs_realloc(vec->file_array, sizeof(*new_file_array) * new_capacity, err);
        VOID_CHECK_ERR;

        vec->file_array = new_file_array;
        vec->capacity = new_capacity;
    }

    jfs_fw_file_transfer(&vec->file_array[vec->count], file_free);
    vec->count += 1;
}

static jfs_fw_file_t *fw_file_vector_to_array(fw_file_vector_t *vec_free, jfs_err_t *err) {
    NULL_FAIL_IF(vec_free->count == 0, JFS_ERR_EMPTY);

    jfs_fw_file_t *file_array = jfs_realloc(vec_free->file_array, sizeof(*file_array) * vec_free->count, err);
    NULL_CHECK_ERR;

    memset(vec_free, 0, sizeof(*vec_free));
    return file_array;
}

static void fw_dir_vector_init(fw_dir_vector_t *vec_init, jfs_err_t *err) {
    jfs_fw_dir_t *new_dir_array = jfs_malloc(sizeof(*new_dir_array) * FW_DIR_VECTOR_DEFAULT_CAPACITY, err);
    VOID_CHECK_ERR;

    vec_init->dir_array = new_dir_array;
    vec_init->capacity = FW_DIR_VECTOR_DEFAULT_CAPACITY;
    vec_init->count = 0;
}

static void fw_dir_vector_free(fw_dir_vector_t *vec_free) {
    if (vec_free->dir_array != NULL) {
        for (size_t i = 0; i < vec_free->count; i++) {
            jfs_fw_dir_free(&vec_free->dir_array[i]);
        }

        free(vec_free->dir_array);
    }

    memset(vec_free, 0, sizeof(*vec_free));
}

static void fw_dir_vector_push(fw_dir_vector_t *vec, jfs_fw_dir_t *dir_free, jfs_err_t *err) {
    if (vec->count >= vec->capacity) {
        const size_t new_capacity = vec->capacity * 2;

        jfs_fw_dir_t *new_dir_array = jfs_realloc(vec->dir_array, sizeof(*new_dir_array) * new_capacity, err);
        VOID_CHECK_ERR;

        vec->dir_array = new_dir_array;
        vec->capacity = new_capacity;
    }

    jfs_fw_dir_transfer(&vec->dir_array[vec->count], dir_free);
    vec->count += 1;
}

static jfs_fw_dir_t *fw_dir_vector_to_array(fw_dir_vector_t *vec_free, jfs_err_t *err) {
    NULL_FAIL_IF(vec_free->count == 0, JFS_ERR_EMPTY);

    jfs_fw_dir_t *dir_array = jfs_realloc(vec_free->dir_array, sizeof(*dir_array) * vec_free->count, err);
    NULL_CHECK_ERR;

    memset(vec_free, 0, sizeof(*vec_free));
    return dir_array;
}

static void fw_scan_dir(DIR *dir, fw_file_vector_t *vec, jfs_err_t *err) {
    struct dirent *ent = NULL;

    errno = 0;
    while ((ent = readdir(dir)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;
        fw_handle_dirent(ent, vec, err);
        if (*err == JFS_ERR_FW_UNSUPPORTED) {
            RES_ERR;
            continue;
        }
        VOID_CHECK_ERR;
    }

    VOID_FAIL_IF(errno != 0, JFS_ERR_SYS);
}

static void fw_handle_dirent(const struct dirent *ent, fw_file_vector_t *vec, jfs_err_t *err) {
    jfs_fw_file_t file = {0};
    fw_file_init(&file, ent, err);
    VOID_CHECK_ERR;

    fw_file_vector_push(vec, &file, err);
    if (*err != JFS_OK) {
        jfs_fw_file_free(&file);
        VOID_RETURN_ERR;
    }
}

static void fw_push_dir_paths(fw_path_vector_t *path_vec, fw_file_vector_t *file_vec, const jfs_fio_path_t *dir_path, jfs_err_t *err) {
    jfs_fio_path_t     push_path = {0};
    jfs_fio_path_buf_t buf = {0};

    for (size_t i = 0; i < file_vec->count; i++) {
        if (file_vec->file_array[i].type == JFS_FW_REG) continue;

        jfs_fio_path_buf_compose(&buf, dir_path, &(file_vec->file_array[i].name), err);
        if (*err == JFS_ERR_FIO_PATH_OVERFLOW) {
            RES_ERR;
            continue;
        }

        jfs_fio_path_init(&push_path, buf.data, err);
        VOID_CHECK_ERR;

        fw_path_vector_push(path_vec, &push_path, err);
        if (*err != JFS_OK) {
            jfs_fio_path_free(&push_path);
            VOID_RETURN_ERR;
        }
    }
}
