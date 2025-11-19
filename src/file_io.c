#include "file_io.h"
#include "error.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

size_t jfs_fio_write(int fd, const void *buf, size_t size, jfs_err_t *err) {
    const uint8_t *write_buf = (uint8_t *) buf;
    size_t         total_written = 0;

    while (total_written < size) {
        const size_t written = jfs_write(fd, write_buf + total_written, size - total_written, err);
        if (*err == JFS_ERR_INTER) continue;
        if (*err != JFS_OK) VAL_RETURN_ERR(total_written);
        total_written += written;
    }

    return total_written;
}

size_t jfs_fio_read(int fd, void *buf, size_t size, jfs_err_t *err) {
    uint8_t *read_buf = (uint8_t *) buf;
    size_t         total_read = 0;

    while (total_read < size) {
        const size_t read = jfs_read(fd, read_buf + total_read, size - total_read, err);
        if (*err == JFS_ERR_INTER) continue;
        if (*err != JFS_OK) VAL_RETURN_ERR(total_read);
        if (read == 0) {
            *err = JFS_ERR_FIO_FILE_END;
            VAL_RETURN_ERR(total_read);
        }
        total_read += read;
    }

    return total_read;
}

void jfs_fio_path_init(jfs_fio_path_t *path_init, const char *path_str, jfs_err_t *err) {
    size_t path_str_len = strlen(path_str);

    if (path_str_len > 1 && path_str[path_str_len - 1] == '/') {
        path_str_len -= 1;
    }
    VOID_FAIL_IF(path_str_len > PATH_MAX, JFS_ERR_FIO_PATH_LEN);

    char *new_str = jfs_malloc(path_str_len + 1, err);
    VOID_CHECK_ERR;

    strlcpy(new_str, path_str, path_str_len + 1);
    path_init->len = path_str_len;
    path_init->str = new_str;
}

void jfs_fio_path_free(jfs_fio_path_t *path_free) {
    free(path_free->str);
    memset(path_free, 0, sizeof(*path_free));
}

void jfs_fio_path_transfer(jfs_fio_path_t *path_init, jfs_fio_path_t *path_free) {
    *path_init = *path_free;
    memset(path_free, 0, sizeof(*path_free));
}

void jfs_fio_name_init(jfs_fio_name_t *name_init, const char *name_str, jfs_err_t *err) {
    size_t name_str_len = strlen(name_str);
    VOID_FAIL_IF(name_str_len > NAME_MAX, JFS_ERR_FIO_NAME_LEN);

    char *new_str = jfs_malloc(name_str_len + 1, err);
    VOID_CHECK_ERR;

    strlcpy(new_str, name_str, name_str_len + 1);
    name_init->len = name_str_len;
    name_init->str = new_str;
}

void jfs_fio_name_free(jfs_fio_name_t *name_free) {
    free(name_free->str);
    memset(name_free, 0, sizeof(*name_free));
}

void jfs_fio_name_transfer(jfs_fio_name_t *name_init, jfs_fio_name_t *name_free) {
    *name_init = *name_free;
    memset(name_free, 0, sizeof(*name_free));
}

void jfs_fio_path_buf_clear(jfs_fio_path_buf_t *buf) {
    buf->len = 0;
    memset(buf->data, 0, sizeof(buf->data));
}

void jfs_fio_path_buf_copy(jfs_fio_path_buf_t *buf, const jfs_fio_path_t *path, jfs_err_t *err) {
    VOID_FAIL_IF(path->len == 0, JFS_ERR_ARG);
    jfs_fio_path_buf_clear(buf);
    strlcpy(buf->data, path->str, sizeof(buf->data));
    buf->len = path->len;
}

void jfs_fio_path_buf_compose(jfs_fio_path_buf_t *buf, const jfs_fio_path_t *path, const jfs_fio_name_t *name, jfs_err_t *err) {
    VOID_FAIL_IF(path->len == 0, JFS_ERR_ARG);
    const size_t new_len = path->len + name->len + 1;
    VOID_FAIL_IF(new_len > PATH_MAX, JFS_ERR_FIO_PATH_OVERFLOW);
    jfs_fio_path_buf_clear(buf);
    snprintf(buf->data, sizeof(buf->data), "%s/%s", path->str, name->str);
    buf->len = new_len;
}
