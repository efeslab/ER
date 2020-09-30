#include "sockets_simulator.h"

char useSymbolicHandler;

static socket_event_handler_t *all_handlers[];

socket_simulator_t __sock_simulator;

void klee_init_sockets_simulator() {
  __sock_simulator.registered_cnt = 0;
  memset(__sock_simulator.handlers, 0, sizeof(__sock_simulator.handlers));
}

/*
 * Application specific socket handler
 */
static memcached_1_5_13_handler_t memcached_1_5_13_handler = {
    .__base = {
        .name = "memcached_1.5.13",
        .init = memcached_1_5_13_handler_init,
        .post_bind = memcached_1_5_13_handler_post_bind,
        .post_listen = memcached_1_5_13_handler_post_listen,
    },
    .client_sock = NULL,
    .server_sock = NULL,
};

static apache_60324_handler_t apache_60324_handler = {
    .__base = {
        .name = "apache-60324",
        .init = apache_60324_handler_init,
        .post_bind = apache_60324_handler_post_bind,
        .post_listen = apache_60324_handler_post_listen,
    },
    .client_sock = NULL,
    .server_sock = NULL,
};

static socket_event_handler_t *all_handlers[] = {
    (socket_event_handler_t *)(&memcached_1_5_13_handler),
    (socket_event_handler_t *)(&apache_60324_handler)
};

void register_predefined_socket_handler(const char *handler_name) {
  int i;
  for (i = 0; i < sizeof(all_handlers) / sizeof(all_handlers[0]); ++i) {
    socket_event_handler_t *hdl = all_handlers[i];
    if (strcmp(handler_name, hdl->name) == 0) {
      __sock_simulator.handlers[__sock_simulator.registered_cnt++] = hdl;
      hdl->init(hdl);
      return;
    }
  }
  posix_debug_msg("Ignore unknown socket handler %s\n", handler_name);
}
