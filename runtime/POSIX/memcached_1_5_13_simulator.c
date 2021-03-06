#include "sockets_simulator.h"

#include <assert.h>
#include <pthread.h>

// memcached 1.5.13 handler
#define MEMCACHED_PORT 11212
void memcached_1_5_13_handler_init(void *self) {
  memcached_1_5_13_handler_t *self_hdl = (memcached_1_5_13_handler_t*)self;
  self_hdl->client_sock = _create_socket(AF_INET, SOCK_STREAM, 0);
};

void memcached_1_5_13_handler_post_bind(void *self, socket_t *sock,
                                        const struct sockaddr *addr,
                                        socklen_t addrlen) {
  memcached_1_5_13_handler_t *self_hdl = (memcached_1_5_13_handler_t*)self;
  // I am only interested in socket bind to DEFAULT_ADDR:MEMCACHED_PORT
  if (sock->domain != AF_INET) {
    return;
  }
  const struct sockaddr_in *inetaddr = (struct sockaddr_in*)addr;
  if (inetaddr->sin_addr.s_addr != __net.net_addr.s_addr) {
    return;
  }
  if (inetaddr->sin_port != htons(MEMCACHED_PORT)) {
    return;
  }
  self_hdl->server_sock = sock;
  posix_debug_msg("memcached_1_5_13 bind handler catch the server socket\n");
}

typedef struct {
  memcached_1_5_13_handler_t *self_hdl;
} memcached_1_5_13_handler_post_listen_arg_t;
memcached_1_5_13_handler_post_listen_arg_t memcached_1_5_13_listen_arg;
static void *memcached_1_5_13_handler_post_listen_newthread(void *_arg) {
    memcached_1_5_13_handler_post_listen_arg_t *arg = (memcached_1_5_13_handler_post_listen_arg_t*)_arg;
    memcached_1_5_13_handler_t *self_hdl = arg->self_hdl;
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr = __net.net_addr;
    server_addr.sin_port = htons(MEMCACHED_PORT);
    char server_addr_str[MAX_SOCK_ADDRSTRLEN];
    _get_sockaddr_str(server_addr_str, sizeof(server_addr_str), AF_INET,
                      (const struct sockaddr*)&server_addr);
    posix_debug_msg("Attempting to connect to the server socket %s\n",
                    server_addr_str);
    int ret;
    ret = _stream_connect(self_hdl->client_sock,
                              (const struct sockaddr *)&server_addr,
                              sizeof(server_addr));
    assert(ret == 0 && "memcached_1_5_13 listen handler fails to connect to "
                       "the server socket");
    unsigned char payload[] = {0x6c, 0x72, 0x75, 0x20, 0x6d, 0x6f, 0x64,
                               0x65, 0x0a, 0x6f, 0xb1, 0xb4, 0x00, 0x6c,
                               0x72, 0x75, 0x64, 0x65, 0xea, 0x07, 0x6d,
                               0x4e, 0x4f, 0x54, 0x5f};
    posix_debug_msg("Attempting to write the payload from the client socket\n");
    if (useSymbolicHandler)
      klee_make_symbolic(payload, sizeof(payload), "memcached1_5_13_payload");
    ret = _write_socket(self_hdl->client_sock, payload, sizeof(payload));
    posix_debug_msg("payload write result %d\n", ret);
    return NULL;
}

void memcached_1_5_13_handler_post_listen(
    void *self, socket_t *sock, __attribute__((unused)) int backlog) {
  memcached_1_5_13_handler_t *self_hdl = (memcached_1_5_13_handler_t *)self;
  if (self_hdl->server_sock && self_hdl->server_sock == sock) {
    memcached_1_5_13_listen_arg.self_hdl = self_hdl;
    pthread_t th;
    int ret = pthread_create(&th, NULL,
                             memcached_1_5_13_handler_post_listen_newthread,
                             &memcached_1_5_13_listen_arg);
    if (ret != 0) {
      posix_debug_msg(
          "memcached_1_5_13 listen handler pthread failed with ret %d\n", ret);
    }
  }
}
