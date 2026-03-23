#ifndef TCP_H
#define TCP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <netinet/in.h>

#define MAX_LINE 512
#define MAX_NEI 64

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
    char net[4];
    char id[3];
    bool joined;
    int listen_fd;

    char self_ip[INET_ADDRSTRLEN];
    uint16_t self_tcp_port;

    char server_ip[INET_ADDRSTRLEN];
    uint16_t server_udp_port;

    neighbor_t nei[MAX_NEI];
    size_t ncount;
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

#endif
