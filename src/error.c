#include "error.h"
#include <asm-generic/errno-base.h>
#include <asm-generic/errno.h>
#include <errno.h>
#include <netdb.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/eventfd.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

const char *jfs_err_str(const jfs_err_t *err) {
    switch (*err) {
#define X(name)              \
    case name: return #name;
        JFS_ERROR_LIST;
#undef X
        default: return "failed to map jfs_error_t to string";
    }
}

void *jfs_malloc(size_t size, jfs_err_t *err) {
    void *allocated_memory = malloc(size);
    NULL_FAIL_IF(allocated_memory == NULL, JFS_ERR_SYS);

    return allocated_memory;
}

void *jfs_realloc(void *ptr, size_t size, jfs_err_t *err) {
    void *allocated_memory = realloc(ptr, size);
    NULL_FAIL_IF(allocated_memory == NULL, JFS_ERR_SYS);

    return allocated_memory;
}

void jfs_lstat(const char *path_str, struct stat *stat_init, jfs_err_t *err) {
    if (lstat(path_str, stat_init) != 0) {
        switch (errno) {
            case EACCES:  *err = JFS_ERR_ACCESS; break;
            case ENOENT:
            case ENOTDIR: *err = JFS_ERR_INVAL_PATH; break;
            default:      *err = JFS_ERR_SYS; break;
        }
        VOID_RETURN_ERR;
    }
}

DIR *jfs_opendir(const char *path_str, jfs_err_t *err) {
    DIR *dir = opendir(path_str);
    if (dir == NULL) {
        switch (errno) {
            case ENOENT:
            case ENOTDIR: *err = JFS_ERR_INVAL_PATH; break;
            case EACCES:  *err = JFS_ERR_ACCESS; break;
            default:      *err = JFS_ERR_SYS; break;
        }
        NULL_RETURN_ERR;
    }

    return dir;
}

void jfs_shutdown(int sock_fd, int how, jfs_err_t *err) {
    if (shutdown(sock_fd, how) != 0) {
        switch (errno) {
            default: *err = JFS_ERR_SYS; break;
        }
        VOID_RETURN_ERR;
    }
}

struct addrinfo *jfs_getaddrinfo(const char *name, const char *port_str, const struct addrinfo *hints, jfs_err_t *err) {
    struct addrinfo *result;
    int              status = getaddrinfo(name, port_str, hints, &result);
    if (status != 0) {
        switch (status) {
            case EAI_AGAIN:  *err = JFS_ERR_AGAIN; break;
            case EAI_FAIL:
            case EAI_NONAME: *err = JFS_ERR_LAN_HOST_UNREACH; break;
            case EAI_SYSTEM: *err = JFS_ERR_SYS; break;
            default:         *err = JFS_ERR_GETADDRINFO; break;
        }
        NULL_RETURN_ERR;
    }

    return result;
}

void jfs_bind(int sock_fd, const struct sockaddr *addr, socklen_t addrlen, jfs_err_t *err) {
    if (bind(sock_fd, addr, addrlen) != 0) {
        switch (errno) {
            default: *err = JFS_ERR_SYS; break;
        }
        VOID_RETURN_ERR;
    }
}

void jfs_listen(int sock_fd, int backlog, jfs_err_t *err) {
    if (listen(sock_fd, backlog) != 0) {
        switch (errno) {
            default: *err = JFS_ERR_SYS; break;
        }
        VOID_RETURN_ERR;
    }
}

int jfs_accept(int sock_fd, struct sockaddr *addr, socklen_t *addrlen, jfs_err_t *err) {
    int accepted_fd = accept(sock_fd, addr, addrlen);
    if (accepted_fd == -1) {
        switch (errno) {
            case ECONNABORTED: *err = JFS_ERR_CONNECTION_ABORT; break;
            case EPROTO:
            case ENOPROTOOPT:
            case EHOSTDOWN:
            case ENONET:
            case EHOSTUNREACH:
            case EOPNOTSUPP:
            case ENETUNREACH:
            case EAGAIN:       *err = JFS_ERR_AGAIN; break;
            case EINTR:        *err = JFS_ERR_INTER; break;
            default:           *err = JFS_ERR_SYS; break;
        }
        VAL_RETURN_ERR(-1);
    }

    return accepted_fd;
}

void jfs_connect(int sock_fd, const struct sockaddr *addr, socklen_t addrlen, jfs_err_t *err) {
    if (connect(sock_fd, addr, addrlen) != 0) {
        switch (errno) {
            case EAGAIN:       *err = JFS_ERR_AGAIN; break;
            case ECONNREFUSED:
            case ETIMEDOUT:
            case ENETUNREACH:  *err = JFS_ERR_LAN_HOST_UNREACH; break;
            case EINTR:        *err = JFS_ERR_INTER; break;
            default:           *err = JFS_ERR_SYS; break;
        }
        VOID_RETURN_ERR;
    }
}

size_t jfs_recv(int sock_fd, void *buf, size_t size, int flags, jfs_err_t *err) {
    ssize_t status = recv(sock_fd, buf, size, flags);
    if (status == -1) {
        switch (errno) {
            case EAGAIN: *err = JFS_ERR_AGAIN; break;
            case EINTR:  *err = JFS_ERR_INTER; break;
            default:     *err = JFS_ERR_SYS; break;
        }
        VAL_RETURN_ERR(0);
    }

    return (size_t) status;
}

size_t jfs_send(int sock_fd, const void *buf, size_t size, int flags, jfs_err_t *err) {
    ssize_t status = send(sock_fd, buf, size, flags);
    if (status == -1) {
        switch (errno) {
            case EAGAIN:     *err = JFS_ERR_AGAIN; break;
            case ECONNRESET: *err = JFS_ERR_CONNECTION_RESET; break;
            case EINTR:      *err = JFS_ERR_INTER; break;
            case EPIPE:      *err = JFS_ERR_PIPE; break;
            default:         *err = JFS_ERR_SYS; break;
        }
        VAL_RETURN_ERR(0);
    }

    return (size_t) status;
}

int jfs_socket(int domain, int type, int protocol, jfs_err_t *err) {
    int new_fd = socket(domain, type, protocol);
    if (new_fd == -1) {
        switch (errno) {
            default: *err = JFS_ERR_SYS; break;
        }
        VAL_RETURN_ERR(-1);
    }

    return new_fd;
}

void jfs_close(int close_fd, jfs_err_t *err) {
    if (close(close_fd) == -1) {
        switch (errno) {
            default: *err = JFS_ERR_SYS; break;
        }
        VOID_RETURN_ERR;
    }
}

void jfs_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr, jfs_err_t *err) {
    if (pthread_mutex_init(mutex, attr) != 0) {
        switch (errno) {
            default: *err = JFS_ERR_SYS; break;
        }
        VOID_RETURN_ERR;
    }
}

void jfs_mutex_destroy(pthread_mutex_t *mutex, jfs_err_t *err) {
    if (pthread_mutex_destroy(mutex) != 0) {
        switch (errno) {
            case EBUSY: *err = JFS_ERR_MUTEX_BUSY; break;
            default:    *err = JFS_ERR_SYS; break;
        }
        VOID_RETURN_ERR;
    }
}

void jfs_mutex_trylock(pthread_mutex_t *mutex, jfs_err_t *err) {
    if (pthread_mutex_trylock(mutex) != 0) {
        switch (errno) {
            case EBUSY: *err = JFS_ERR_MUTEX_BUSY; break;
            default:    *err = JFS_ERR_SYS; break;
        }
        VOID_RETURN_ERR;
    }
}

void jfs_cond_destroy(pthread_cond_t *cond, jfs_err_t *err) {
    if (pthread_cond_destroy(cond) != 0) {
        switch (errno) {
            case EBUSY: *err = JFS_ERR_COND_BUSY; break;
            default:    *err = JFS_ERR_SYS; break;
        }
        VOID_RETURN_ERR;
    }
}

void jfs_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex, const struct timespec *time, jfs_err_t *err) {
    if (pthread_cond_timedwait(cond, mutex, time) != 0) {
        switch (errno) {
            case ETIMEDOUT: *err = JFS_ERR_COND_TIMED_OUT; break;
            default:        *err = JFS_ERR_SYS; break;
        }
        VOID_RETURN_ERR;
    }
}

int jfs_eventfd(unsigned int initval, int flags, jfs_err_t *err) {
    int event_fd = eventfd(initval, flags);
    if (event_fd == -1) {
        switch (errno) {
            default: *err = JFS_ERR_SYS; break;
        }
        VAL_RETURN_ERR(-1);
    }
    return event_fd;
}

size_t jfs_read(int fd, void *buf, size_t size, jfs_err_t *err) {
    ssize_t status = read(fd, buf, size);
    if (status == -1) {
        switch (errno) {
            case EAGAIN: *err = JFS_ERR_AGAIN; break;
            case EINTR:  *err = JFS_ERR_INTER; break;
            default:     *err = JFS_ERR_SYS; break;
        }
        VAL_RETURN_ERR(0);
    }
    return (size_t) status;
}

size_t jfs_write(int fd, const void *buf, size_t size, jfs_err_t *err) {
    ssize_t status = write(fd, buf, size);
    if (status == -1) {
        switch (errno) {
            case EAGAIN: *err = JFS_ERR_AGAIN; break;
            case EINTR:  *err = JFS_ERR_INTER; break;
            case EPIPE:  *err = JFS_ERR_PIPE; break;
            default:     *err = JFS_ERR_SYS; break;
        }
        VAL_RETURN_ERR(0);
    }
    return (size_t) status;
}

void *jfs_mmap(void *addr, size_t len, int prot, int flags, int fd, off_t off, jfs_err_t *err) {
    void *mem = mmap(addr, len, prot, flags, fd, off);
    if (mem == MAP_FAILED) {
        switch (errno) {
            default: *err = JFS_ERR_SYS; break;
        }
        NULL_RETURN_ERR;
    }
    return mem;
}

void *jfs_aligned_alloc(size_t align, size_t size, jfs_err_t *err) {
    void *mem = NULL;
    int   status = posix_memalign(&mem, align, size);
    if (status != 0) {
        *err = JFS_ERR_SYS;
        NULL_RETURN_ERR;
    }
    return mem;
}
