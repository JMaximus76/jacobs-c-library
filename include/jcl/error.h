#ifndef JCL_ERROR_H
#define JCL_ERROR_H

#include <dirent.h>
#include <errno.h>
#include <netdb.h>
#include <string.h>
#include <sys/stat.h>


#define VOID_RETURN_ERR \
    do {                \
        return;         \
    } while (0)

#define NULL_RETURN_ERR \
    do {                \
        return NULL;    \
    } while (0)

#define VAL_RETURN_ERR(val_var) \
    do {                        \
        return (val_var);       \
    } while (0)

#define VOID_CHECK_ERR        \
    do {                      \
        if (*err != JCL_OK) { \
            VOID_RETURN_ERR;  \
        }                     \
    } while (0)

#define NULL_CHECK_ERR        \
    do {                      \
        if (*err != JCL_OK) { \
            NULL_RETURN_ERR;  \
        }                     \
    } while (0)

#define VAL_CHECK_ERR(val_var)       \
    do {                             \
        if (*err != JCL_OK) {        \
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
        if (*err != JCL_OK) {   \
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
        }                           \
    } while (0)

#define RESS_ERR *err = JCL_OK

#define WUR __attribute__((warn_unused_result))

typedef enum {
    JCL_OK = 1,
    JCL_ERR_SYS,
    JCL_ERR_INTER,
    JCL_ERR_AGAIN,
    JCL_ERR_ACCESS,
    JCL_ERR_INVAL_PATH,
    JCL_ERR_ARG,
    JCL_ERR_EMPTY,
    JCL_ERR_FULL,
    JCL_ERR_BAD_CONF,
    JCL_ERR_GETADDRINFO,
    JCL_ERR_LAN_HOST_UNREACH,
    JCL_ERR_CONNECTION_ABORT,
    JCL_ERR_CONNECTION_RESET,
    JCL_ERR_PIPE,
    JCL_ERR_MUTEX_BUSY,
    JCL_ERR_COND_BUSY,
    JCL_ERR_COND_TIMED_OUT,
} jcl_err_t;

void            *jcl_malloc(size_t size, jcl_err_t *err) WUR;
void            *jcl_realloc(void *ptr, size_t size, jcl_err_t *err) WUR;
void             jcl_lstat(const char *path, struct stat *stat_init, jcl_err_t *err);
DIR             *jcl_opendir(const char *path, jcl_err_t *err) WUR;
void             jcl_shutdown(int sock_fd, int how, jcl_err_t *err);
struct addrinfo *jcl_getaddrinfo(const char *name, const char *port_str, const struct addrinfo *hints, jcl_err_t *err) WUR;
void             jcl_bind(int sock_fd, const struct sockaddr *addr, socklen_t addrlen, jcl_err_t *err);
void             jcl_listen(int sock_fd, int backlog, jcl_err_t *err);
int              jcl_accept(int sock_fd, struct sockaddr *addr, socklen_t *addrlen, jcl_err_t *err) WUR;
void             jcl_connect(int sock_fd, const struct sockaddr *addr, socklen_t addrlen, jcl_err_t *err);
size_t           jcl_recv(int sock_fd, void *buf, size_t size, int flags, jcl_err_t *err) WUR;
size_t           jcl_send(int sock_fd, const void *buf, size_t size, int flags, jcl_err_t *err) WUR;
int              jcl_socket(int domain, int type, int protocol, jcl_err_t *err) WUR;
void             jcl_close(int close_fd, jcl_err_t *err);
void             jcl_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr, jcl_err_t *err);
void             jcl_mutex_destroy(pthread_mutex_t *mutex, jcl_err_t *err);
void             jcl_mutex_trylock(pthread_mutex_t *mutex, jcl_err_t *err);
void             jcl_cond_destroy(pthread_cond_t *cond, jcl_err_t *err);
void             jcl_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex, const struct timespec *time, jcl_err_t *err);
int              jcl_eventfd(unsigned int initval, int flags, jcl_err_t *err) WUR;
size_t           jcl_read(int fd, void *buf, size_t size, jcl_err_t *err) WUR;
size_t           jcl_write(int fd, const void *buf, size_t size, jcl_err_t *err) WUR;
void            *jcl_mmap(void *addr, size_t len, int prot, int flags, int fd, off_t off, jcl_err_t *err) WUR;
void            *jcl_aligned_alloc(size_t align, size_t size, jcl_err_t *err) WUR;

#endif
