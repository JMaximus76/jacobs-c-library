#ifndef JCL_NET_SOCKET_H
#define JCL_NET_SOCKET_H

#include "error.h"
#include <arpa/inet.h>

enum {
    
}

typedef struct jcl_ns_socket jcl_ns_socket_t;

struct jcl_ns_socket;

jcl_ns_socket_t *jcl_ns_socket_create(jcl_err_t *err) WUR;
void             jcl_ns_socket_open(jcl_ns_socket_t *sock, jcl_err_t *err);
void             jcl_ns_socket_close(jcl_ns_socket_t *sock, jcl_err_t *err);
void             jcl_ns_socket_destroy(jcl_ns_socket_t **sock_give);

void             jcl_ns_socket_shutdown(const jcl_ns_socket_t *sock, jcl_err_t *err);
void             jcl_ns_socket_set_ip(jcl_ns_socket_t *sock, uint16_t server_port, const char *server_ip, jcl_err_t *err);
void             jcl_ns_socket_set_hostname(jcl_ns_socket_t *sock, uint16_t server_port, const char *hostname, jcl_err_t *err);
void             jcl_ns_socket_bind(const jcl_ns_socket_t *sock, jcl_err_t *err);
void             jcl_ns_socket_listen(const jcl_ns_socket_t *sock, jcl_err_t *err);
jcl_ns_socket_t *jcl_ns_socket_accept(const jcl_ns_socket_t *sock, jcl_err_t *err) WUR;
void             jcl_ns_socket_connect(const jcl_ns_socket_t *sock, jcl_err_t *err);
size_t           jcl_ns_socket_recv(const jcl_ns_socket_t *sock, void *buf, size_t buf_size, jcl_err_t *err);
size_t           jcl_ns_socket_send(const jcl_ns_socket_t *sock, const void *buf, size_t buf_size, int flags, jcl_err_t *err);

#endif
