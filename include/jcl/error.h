#ifndef JFS_ERROR_H
#define JFS_ERROR_H

#include <dirent.h>
#include <errno.h>
#include <netdb.h>
#include <string.h>
#include <sys/stat.h>

// TEMP TEMP TEMP TEMP TEMP TEMP TEMP TEMP TEMP TEMP TEMP TEMP
#include <stdio.h>
#define LOG_STR "\033[1;31m%s\033[0m: \033[35m%s\033[0m  errno: %s\n"
#define RES_STR "\033[1;36m%s\033[0m: \033[35m%s\033[0m\n"

#define LOG_ERR printf(LOG_STR, jfs_err_str(err), __func__, strerror(errno))
#define RES_ERR                                      \
    do {                                             \
        printf(RES_STR, jfs_err_str(err), __func__); \
        *err = JFS_OK;                               \
    } while (0)

#define IGNORE_ERR *err = JFS_OK;
// TEMP TEMP TEMP TEMP TEMP TEMP TEMP TEMP TEMP TEMP TEMP TEMP

#define VOID_RETURN_ERR \
    do {                \
        LOG_ERR;        \
        return;         \
    } while (0)

#define NULL_RETURN_ERR \
    do {                \
        LOG_ERR;        \
        return NULL;    \
    } while (0)

#define VAL_RETURN_ERR(val_var) \
    do {                        \
        LOG_ERR;                \
        return (val_var);       \
    } while (0)

#define VOID_CHECK_ERR        \
    do {                      \
        if (*err != JFS_OK) { \
            VOID_RETURN_ERR;  \
        }                     \
    } while (0)

#define NULL_CHECK_ERR        \
    do {                      \
        if (*err != JFS_OK) { \
            NULL_RETURN_ERR;  \
        }                     \
    } while (0)

#define VAL_CHECK_ERR(val_var)       \
    do {                             \
        if (*err != JFS_OK) {        \
            VAL_RETURN_ERR(val_var); \
        }                            \
    } while (0)

#define VOID_FAIL_IF(cond_expr, err_var) \
    do {                                 \
        if ((cond_expr)) {               \
            *err = err_var;              \
            VOID_RETURN_ERR;             \
        }                                \
    } while (0)

#define NULL_FAIL_IF(cond_expr, err_var) \
    do {                                 \
        if ((cond_expr)) {               \
            *err = err_var;              \
            NULL_RETURN_ERR;             \
        }                                \
    } while (0)

#define VAL_FAIL_IF(cond_expr, err_var, val_var) \
    do {                                         \
        if ((cond_expr)) {                       \
            *err = err_var;                      \
            VAL_RETURN_ERR(val_var);             \
        }                                        \
    } while (0)

#define GOTO_IF_ERR(label_name) \
    do {                        \
        if (*err != JFS_OK) {   \
            goto label_name;    \
        }                       \
    } while (0)

#define GOTO_WITH_ERR(label_name, err_var) \
    do {                                   \
        *err = err_var;                    \
        goto label_name;                   \
    } while (0)

#define REMAP_ERR(from_var, to_var) \
    do {                            \
        if (*err == (from_var)) {   \
            *err = (to_var);        \
            printf("remap err\n");  \
        }                           \
    } while (0)

#define WUR __attribute__((warn_unused_result))

#define JFS_ERROR_LIST             \
    X(JFS_OK)                      \
    X(JFS_ERR_SYS)                 \
    X(JFS_ERR_INTER)               \
    X(JFS_ERR_AGAIN)               \
    X(JFS_ERR_ACCESS)              \
    X(JFS_ERR_INVAL_PATH)          \
    X(JFS_ERR_ARG)                 \
    X(JFS_ERR_EMPTY)               \
    X(JFS_ERR_FULL)                \
    X(JFS_ERR_BAD_CONF)            \
    X(JFS_ERR_GETADDRINFO)         \
    X(JFS_ERR_LAN_HOST_UNREACH)    \
    X(JFS_ERR_CONNECTION_ABORT)    \
    X(JFS_ERR_CONNECTION_RESET)    \
    X(JFS_ERR_PIPE)                \
    X(JFS_ERR_MUTEX_BUSY)          \
    X(JFS_ERR_COND_BUSY)           \
    X(JFS_ERR_COND_TIMED_OUT)      \
    X(JFS_ERR_FIO_PATH_LEN)        \
    X(JFS_ERR_FIO_NAME_LEN)        \
    X(JFS_ERR_FIO_PATH_OVERFLOW)   \
    X(JFS_ERR_FIO_FILE_END)        \
    X(JFS_ERR_FW_STATE)            \
    X(JFS_ERR_FW_SKIP)             \
    X(JFS_ERR_FW_FAIL)             \
    X(JFS_ERR_FW_UNSUPPORTED)      \
    X(JFS_ERR_FW_UNKNOWN)          \
    X(JFS_ERR_NS_BAD_ACCEPT)       \
    X(JFS_ERR_NS_BAD_ADDR)         \
    X(JFS_ERR_NS_CONNECTION_CLOSE) \
    X(JFS_ERR_BST_BAD_KEY)

typedef enum {
#define X(name) name,
    JFS_ERROR_LIST
#undef X
} jfs_err_t;

const char *jfs_err_str(const jfs_err_t *err);

void            *jfs_malloc(size_t size, jfs_err_t *err) WUR;
void            *jfs_realloc(void *ptr, size_t size, jfs_err_t *err) WUR;
void             jfs_lstat(const char *path, struct stat *stat_init, jfs_err_t *err);
DIR             *jfs_opendir(const char *path, jfs_err_t *err) WUR;
void             jfs_shutdown(int sock_fd, int how, jfs_err_t *err);
struct addrinfo *jfs_getaddrinfo(const char *name, const char *port_str, const struct addrinfo *hints, jfs_err_t *err) WUR;
void             jfs_bind(int sock_fd, const struct sockaddr *addr, socklen_t addrlen, jfs_err_t *err);
void             jfs_listen(int sock_fd, int backlog, jfs_err_t *err);
int              jfs_accept(int sock_fd, struct sockaddr *addr, socklen_t *addrlen, jfs_err_t *err) WUR;
void             jfs_connect(int sock_fd, const struct sockaddr *addr, socklen_t addrlen, jfs_err_t *err);
size_t           jfs_recv(int sock_fd, void *buf, size_t size, int flags, jfs_err_t *err) WUR;
size_t           jfs_send(int sock_fd, const void *buf, size_t size, int flags, jfs_err_t *err) WUR;
int              jfs_socket(int domain, int type, int protocol, jfs_err_t *err) WUR;
void             jfs_close(int close_fd, jfs_err_t *err);
void             jfs_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr, jfs_err_t *err);
void             jfs_mutex_destroy(pthread_mutex_t *mutex, jfs_err_t *err);
void             jfs_mutex_trylock(pthread_mutex_t *mutex, jfs_err_t *err);
void             jfs_cond_destroy(pthread_cond_t *cond, jfs_err_t *err);
void             jfs_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex, const struct timespec *time, jfs_err_t *err);
int              jfs_eventfd(unsigned int initval, int flags, jfs_err_t *err) WUR;
size_t           jfs_read(int fd, void *buf, size_t size, jfs_err_t *err) WUR;
size_t           jfs_write(int fd, const void *buf, size_t size, jfs_err_t *err) WUR;
void            *jfs_mmap(void *addr, size_t len, int prot, int flags, int fd, off_t off, jfs_err_t *err) WUR;
void            *jfs_aligned_alloc(size_t align, size_t size, jfs_err_t *err) WUR;

#endif
