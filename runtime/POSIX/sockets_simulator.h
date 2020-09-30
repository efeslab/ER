#ifndef SOCKETS_SIMULATOR_H_
#define SOCKETS_SIMULATOR_H_
#include "sockets.h"
// This flag denotes if sockets simulator is used during a symbolic replay
// Configurable in klee_init_env (option "-symbolic-sock-handler")
extern char useSymbolicHandler;
// a base structure every customizd simulator/handler should derive from
// NOTE that you should always put this structure to the first field of your
// customized structure
typedef struct {
  const char *name;
  void (*init)(void *self);
  void (*post_bind)(void *self, socket_t *sock,
                           const struct sockaddr *addr, socklen_t addrlen);
  void (*post_listen)(void *self, socket_t *sock, int backlog);
} socket_event_handler_t;

typedef struct {
  socket_event_handler_t __base;
  socket_t *client_sock;
  socket_t *server_sock;
} client_socket_handler_t;

typedef struct {
  unsigned int registered_cnt;
  socket_event_handler_t  *handlers[MAX_SOCK_EVT_HANDLE];
} socket_simulator_t;
extern socket_simulator_t __sock_simulator;
void klee_init_sockets_simulator();
// @return 1 if register successfully, 0 if not
void register_predefined_socket_handler(const char *handler_name);

#define TRIGGER_SOCKET_HANDLER(handle, ...)                                    \
  do {                                                                         \
    int i;                                                                     \
    for (i = 0; i < __sock_simulator.registered_cnt; ++i) {                    \
      socket_event_handler_t *hdl = __sock_simulator.handlers[i];              \
      if (hdl->handle)                                                         \
        hdl->handle(hdl, __VA_ARGS__);                                         \
    }                                                                          \
  } while (0)

#define DECLARE_SOCKET_HANDLER_FUNC(name)                                      \
  void name##_handler_init(void *self);                                        \
  void name##_handler_post_bind(void *self, socket_t *sock,                    \
                                const struct sockaddr *addr,                   \
                                socklen_t addrlen);                            \
  void name##_handler_post_listen(void *self, socket_t *sock,                  \
                                  __attribute__((unused)) int backlog)
/*
 * sockets simulator for different applications
 */
// memcached 1.5.13
typedef client_socket_handler_t memcached_1_5_13_handler_t;
DECLARE_SOCKET_HANDLER_FUNC(memcached_1_5_13);
// apache-60324
typedef client_socket_handler_t apache_60324_handler_t;
DECLARE_SOCKET_HANDLER_FUNC(apache_60324);
#endif // SOCKETS_SIMULATOR_H_
