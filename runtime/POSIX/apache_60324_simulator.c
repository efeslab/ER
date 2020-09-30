#include "sockets_simulator.h"

#include <assert.h>
#include <pthread.h>

// apache-60324 handler
#define HTTPD_PORT 40880
void apache_60324_handler_init(void *self) {
  apache_60324_handler_t *self_hdl = (apache_60324_handler_t*)self;
  self_hdl->client_sock = _create_socket(AF_INET, SOCK_STREAM, 0);
};

void apache_60324_handler_post_bind(void *self, socket_t *sock,
                                    const struct sockaddr *addr,
                                    socklen_t addrlen) {
  apache_60324_handler_t *self_hdl = (apache_60324_handler_t*)self;
  // I am only interested in socket bind to DEFAULT_ADDR:HTTPD_PORT
  if (sock->domain != AF_INET) {
    return;
  }
  const struct sockaddr_in *inetaddr = (struct sockaddr_in*)addr;
  if (inetaddr->sin_addr.s_addr != __net.net_addr.s_addr) {
    return;
  }
  if (inetaddr->sin_port != htons(HTTPD_PORT)) {
    return;
  }
  self_hdl->server_sock = sock;
  posix_debug_msg("apache_60324 bind handler catch the server socket\n");
}

typedef struct {
  apache_60324_handler_t *self_hdl;
} apache_60324_handler_post_listen_arg_t;
apache_60324_handler_post_listen_arg_t apache_60324_listen_arg;
static void *apache_60324_handler_post_listen_newthread(void *_arg) {
    apache_60324_handler_post_listen_arg_t *arg = (apache_60324_handler_post_listen_arg_t*)_arg;
    apache_60324_handler_t *self_hdl = arg->self_hdl;
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr = __net.net_addr;
    server_addr.sin_port = htons(HTTPD_PORT);
    char server_addr_str[MAX_SOCK_ADDRSTRLEN];
    _get_sockaddr_str(server_addr_str, sizeof(server_addr_str), AF_INET,
                      (const struct sockaddr*)&server_addr);
    posix_debug_msg("Attempting to connect to the server socket %s\n",
                    server_addr_str);
    int ret;
    ret = _stream_connect(self_hdl->client_sock,
                              (const struct sockaddr *)&server_addr,
                              sizeof(server_addr));
    assert(ret == 0 && "apache_60324 listen handler fails to connect to "
                       "the server socket");
    unsigned char payload[] =
      "GET /rest/running/config/port/port-name/user_portnumber_12 HTTP/1.1\r\n"
      "Host: 127.0.0.1:40880\r\n"
      "User-Agent: curl/7.58.0\r\n"
      "Accept: */*\r\n"
      "\r\n";
    posix_debug_msg("Attempting to write the payload from the client socket\n");
    if (useSymbolicHandler)
      klee_make_symbolic(payload, sizeof(payload), "apache_60324_payload");
    ret = _write_socket(self_hdl->client_sock, payload, sizeof(payload));
    posix_debug_msg("payload write result %d\n", ret);
    return NULL;
}

void apache_60324_handler_post_listen(
    void *self, socket_t *sock, __attribute__((unused)) int backlog) {
  apache_60324_handler_t *self_hdl = (apache_60324_handler_t *)self;
  if (self_hdl->server_sock && self_hdl->server_sock == sock) {
    apache_60324_listen_arg.self_hdl = self_hdl;
    pthread_t th;
    int ret = pthread_create(&th, NULL,
                             apache_60324_handler_post_listen_newthread,
                             &apache_60324_listen_arg);
    if (ret != 0) {
      posix_debug_msg(
          "apache_60324 listen handler pthread failed with ret %d\n", ret);
    }
  }
}
