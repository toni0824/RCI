#ifndef TCP_H
#define TCP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <netinet/in.h>

#define MAX_LINE 512
#define MAX_NEI 64
#define MAX_DEST 128
#define ROUTE_INF 1000000

typedef enum {
    ROUTE_STATE_EXPEDITION = 0,
    ROUTE_STATE_COORDINATION = 1
} route_state_t;

typedef struct {
    int fd;
    bool active;
    bool handshake;
    char id[3];
    char ip[INET_ADDRSTRLEN];
    uint16_t port;
    char buf[1024];
    size_t len;
} neighbor_t;

typedef struct {
    char dest[3];
    bool present;
    bool valid;
    int distance;
    char successor[3];
    route_state_t state;
    char trigger_neighbor[3];
    int advertised[MAX_NEI];
    bool waiting[MAX_NEI];
} route_entry_t;

typedef struct {
    char net[4];
    char id[3];
    bool joined;
    bool direct_join;
    int listen_fd;

    char self_ip[INET_ADDRSTRLEN];
    uint16_t self_tcp_port;

    char server_ip[INET_ADDRSTRLEN];
    uint16_t server_udp_port;

    neighbor_t nei[MAX_NEI];
    size_t ncount;
    route_entry_t routes[MAX_DEST];
    size_t route_count;
    bool monitor_enabled;
    bool running;
} state_t;

int make_listener(uint16_t port);
void cmd_show_neighbors(state_t *st);
void cmd_direct_add_edge(state_t *st, const char *id, const char *ip, uint16_t port);
void cmd_remove_edge(state_t *st, const char *id);
void accept_conn(state_t *st);
void pump_neighbor(state_t *st, neighbor_t *n);
neighbor_t *find_by_id(state_t *st, const char id[3]);
neighbor_t *alloc_neighbor(state_t *st);
void close_nei(neighbor_t *n);
void send_line(int fd, const char *fmt, ...);
void cmd_announce(state_t *st);
void cmd_show_routing(state_t *st, const char *dest);
void cmd_message(state_t *st, const char *dest, const char *text);
void cmd_monitor(state_t *st, bool enabled);
void notify_neighbor_confirmed(state_t *st, neighbor_t *n);
void notify_neighbor_closed(state_t *st, neighbor_t *n);

#endif
