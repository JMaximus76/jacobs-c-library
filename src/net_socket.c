#include <jcl/net_socket.h>
#include <jcl/error.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>

#define LISTEN_BACK_LOG 5

struct jcl_ns_socket {
    int                fd;
    struct sockaddr_in addr;
};

jcl_ns_socket_t *jcl_ns_socket_create(jcl_err_t *err) {
    jcl_ns_socket_t *sock = jcl_malloc(sizeof(*sock), err);
    NULL_CHECK_ERR;

    sock->fd = -1;
    memset(&sock->addr, 0, sizeof(sock->addr));
    return sock;
}

void jcl_ns_socket_open(jcl_ns_socket_t *sock, jcl_err_t *err) {
    int new_fd = jcl_socket(AF_INET, SOCK_STREAM, 0, err);
    VOID_CHECK_ERR;
    sock->fd = new_fd;
}

void jcl_ns_socket_close(jcl_ns_socket_t *sock, jcl_err_t *err) {
    if (sock == NULL || sock->fd == -1) return;
    jcl_close(sock->fd, err);
    sock->fd = -1;
    VOID_CHECK_ERR;
}

void jcl_ns_socket_destroy(jcl_ns_socket_t **sock_give) {
    if (sock_give == NULL || *sock_give == NULL) return;
    if ((*sock_give)->fd != -1) close((*sock_give)->fd);
    free(*sock_give);
    *sock_give = NULL;
}

void jcl_ns_socket_shutdown(const jcl_ns_socket_t *sock, jcl_err_t *err) {
    jcl_shutdown(sock->fd, SHUT_WR, err);
    VOID_CHECK_ERR;
}

void jcl_ns_socket_set_ip(jcl_ns_socket_t *sock, uint16_t server_port, const char *server_ip, jcl_err_t *err) {
    VOID_FAIL_IF(server_port <= 1024, JCL_ERR_ARG);

    struct sockaddr_in addr_in = {0};
    addr_in.sin_family = AF_INET;
    addr_in.sin_port = htons(server_port);
    int status = inet_pton(AF_INET, server_ip, &addr_in.sin_addr);
    VOID_FAIL_IF(status != 1, JCL_ERR_ARG);

    sock->addr = addr_in;
}

void jcl_ns_socket_set_hostname(jcl_ns_socket_t *sock, uint16_t server_port, const char *hostname, jcl_err_t *err) {
    VOID_FAIL_IF(server_port <= 1024, JCL_ERR_ARG);

    struct addrinfo hints = {0};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    char port_str[NI_MAXSERV];
    snprintf(port_str, sizeof(port_str), "%d", server_port);

    struct addrinfo *result = jcl_getaddrinfo(hostname, port_str, &hints, err);
    VOID_CHECK_ERR;

    if (result->ai_family != AF_INET || result->ai_addrlen != sizeof(struct sockaddr_in)) {
        freeaddrinfo(result);
        *err = JCL_ERR_NS_BAD_ADDR;
        VOID_RETURN_ERR;
    }

    memcpy(&sock->addr, result->ai_addr, result->ai_addrlen);
    freeaddrinfo(result);
}

void jcl_ns_socket_bind(const jcl_ns_socket_t *sock, jcl_err_t *err) {
    do {
        if (*err == JCL_ERR_INTER) RES_ERR;
        jcl_bind(sock->fd, (struct sockaddr *) &sock->addr, sizeof(sock->addr), err);
    } while (*err == JCL_ERR_INTER);
    VOID_CHECK_ERR;
}

void jcl_ns_socket_listen(const jcl_ns_socket_t *sock, jcl_err_t *err) {
    jcl_listen(sock->fd, LISTEN_BACK_LOG, err);
    VOID_CHECK_ERR;
}

jcl_ns_socket_t *jcl_ns_socket_accept(const jcl_ns_socket_t *sock, jcl_err_t *err) {
    jcl_ns_socket_t *accept_sock = jcl_ns_socket_create(err);
    NULL_CHECK_ERR;

    socklen_t addrlen = sizeof(accept_sock->addr);
    do {
        if (*err == JCL_ERR_INTER) RES_ERR;
        accept_sock->fd = jcl_accept(sock->fd, (struct sockaddr *) &accept_sock->addr, &addrlen, err);
    } while (*err == JCL_ERR_INTER);

    if (*err != jcl_OK) {
        jcl_ns_socket_destroy(&accept_sock);
        NULL_RETURN_ERR;
    }

    if (accept_sock->addr.sin_family != AF_INET || addrlen != sizeof(struct sockaddr_in)) {
        *err = JCL_ERR_NS_BAD_ACCEPT;
        jcl_ns_socket_destroy(&accept_sock);
        NULL_RETURN_ERR;

    }

    return accept_sock;
}

void jcl_ns_socket_connect(const jcl_ns_socket_t *sock, jcl_err_t *err) {
    do {
        if (*err == JCL_ERR_INTER) RES_ERR;
        jcl_connect(sock->fd, (struct sockaddr *) &sock->addr, sizeof(sock->addr), err);
    } while (*err == JCL_ERR_INTER);
    VOID_CHECK_ERR;
}

size_t jcl_ns_socket_recv(const jcl_ns_socket_t *sock, void *buf, size_t buf_size, jcl_err_t *err) {
    uint8_t *recv_buf = (uint8_t *) buf;
    size_t   total_received = 0;

    while (total_received < buf_size) {
        const size_t size_received = jcl_recv(sock->fd, recv_buf + total_received, buf_size - total_received, 0, err);
        if (*err == JCL_ERR_INTER) {
            RES_ERR;
            continue;
        }
        VAL_CHECK_ERR(total_received);

        VAL_FAIL_IF(size_received == 0, JCL_ERR_NS_CONNECTION_CLOSE, total_received);
        total_received += size_received;
    }

    return total_received;
}

size_t jcl_ns_socket_send(const jcl_ns_socket_t *sock, const void *buf, size_t buf_size, int flags, jcl_err_t *err) {
    const uint8_t *send_buf = (uint8_t *) buf;
    size_t         total_sent = 0;

    while (total_sent < buf_size) {
        const size_t size_sent = jcl_send(sock->fd, send_buf + total_sent, buf_size - total_sent, MSG_NOSIGNAL | flags, err);

        if (*err == JCL_ERR_INTER) {
            RES_ERR;
            continue;
        }
        VAL_CHECK_ERR(total_sent);

        total_sent += size_sent;
    }

    return total_sent;
}
