#include "tcp.h"

#include <arpa/inet.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <unistd.h>

static void trim_local(char *s) {
    if (!s) return;
    size_t l = strlen(s);
    while (l && (s[l - 1] == '\n' || s[l - 1] == '\r')) s[--l] = 0;
}

static int split_local(char *line, char *tok[], int max) {
    int c = 0;
    char *p = line;
    while (*p && c < max) {
        while (*p && *p <= ' ') p++;
        if (!*p) break;
        tok[c++] = p;
        while (*p && *p > ' ') p++;
        if (!*p) break;
        *p++ = 0;
    }
    return c;
}

static void reset_neighbor(neighbor_t *n) {
    if (!n) return;
    memset(n, 0, sizeof(*n));
    n->fd = -1;
}

neighbor_t *alloc_neighbor(state_t *st) {
    for (size_t i = 0; i < st->ncount; i++) {
        if (!st->nei[i].active) {
            reset_neighbor(&st->nei[i]);
            st->nei[i].active = true;
            return &st->nei[i];
        }
    }

    if (st->ncount >= MAX_NEI) return NULL;

    neighbor_t *n = &st->nei[st->ncount++];
    reset_neighbor(n);
    n->active = true;
    return n;
}

neighbor_t *find_by_id(state_t *st, const char id[3]) {
    for (size_t i = 0; i < st->ncount; i++) {
        if (st->nei[i].active && strncmp(st->nei[i].id, id, 2) == 0) return &st->nei[i];
    }
    return NULL;
}

void close_nei(neighbor_t *n) {
    if (!n) return;
    if (n->fd >= 0) close(n->fd);
    reset_neighbor(n);
}

int make_listener(uint16_t port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    int opt = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        close(fd);
        return -1;
    }

    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port = htons(port);
    a.sin_addr.s_addr = INADDR_ANY;

    if (bind(fd, (struct sockaddr *)&a, sizeof(a)) < 0) {
        close(fd);
        return -1;
    }

    if (listen(fd, 16) < 0) {
        close(fd);
        return -1;
    }

    return fd;
}

void send_line(int fd, const char *fmt, ...) {
    if (fd < 0) return;

    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    strncat(buf, "\n", sizeof(buf) - strlen(buf) - 1);
    send(fd, buf, strlen(buf), 0);
}

static void handle_msg(state_t *st, neighbor_t *n, const char *line) {
    (void)st;

    char work[MAX_LINE];
    strncpy(work, line, sizeof(work) - 1);
    work[sizeof(work) - 1] = 0;

    char *t[4];
    int argc = split_local(work, t, 4);
    if (argc == 0) return;

    if (strcasecmp(t[0], "NEIGHBOR") == 0 && argc >= 2) {
        char new_id[3] = {0};
        strncpy(new_id, t[1], 2);

        neighbor_t *other = find_by_id(st, new_id);
        if (other && other != n) {
            printf("[warn] vizinho duplicado %s, a fechar ligação nova\n", new_id);
            close_nei(n);
            return;
        }

        strncpy(n->id, new_id, 2);
        n->id[2] = 0;
        n->handshake = true;
        printf("Novo vizinho: %s (%s:%u)\n", n->id, n->ip, n->port);
    }
}

void pump_neighbor(state_t *st, neighbor_t *n) {
    char tmp[256];
    ssize_t r = recv(n->fd, tmp, sizeof(tmp), 0);

    if (r <= 0) {
        printf("Vizinho %s desligou.\n", n->id[0] ? n->id : "??");
        close_nei(n);
        return;
    }

    if (n->len + (size_t)r >= sizeof(n->buf)) n->len = 0;

    memcpy(n->buf + n->len, tmp, (size_t)r);
    n->len += (size_t)r;

    size_t start = 0;
    for (size_t i = 0; i < n->len; i++) {
        if (n->buf[i] == '\n') {
            size_t l = i - start;
            char line[MAX_LINE];
            if (l >= sizeof(line)) l = sizeof(line) - 1;
            memcpy(line, n->buf + start, l);
            line[l] = 0;
            trim_local(line);
            handle_msg(st, n, line);
            start = i + 1;
        }
    }

    if (start > 0) {
        memmove(n->buf, n->buf + start, n->len - start);
        n->len -= start;
    }
}

static int connect_to_neighbor(state_t *st, const char *id, const char *ip, uint16_t port) {
    if (!st->joined) {
        printf("Node isn't registered on any network.\n");
        return -1;
    }

    if (strlen(id) != 2) {
        printf("ID inválido.\n");
        return -1;
    }

    if (find_by_id(st, id)) {
        printf("Já existe aresta com %s.\n", id);
        return -1;
    }

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return -1;
    }

    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &a.sin_addr) != 1) {
        printf("IP inválido.\n");
        close(fd);
        return -1;
    }

    printf("A tentar ligar a %s (%s:%u)...\n", id, ip, port);

    if (connect(fd, (struct sockaddr *)&a, sizeof(a)) < 0) {
        perror("connect");
        close(fd);
        return -1;
    }

    neighbor_t *n = alloc_neighbor(st);
    if (!n) {
        close(fd);
        printf("Limite de vizinhos.\n");
        return -1;
    }

    n->fd = fd;
    n->active = true;
    n->handshake = false;
    strncpy(n->ip, ip, sizeof(n->ip) - 1);
    n->ip[sizeof(n->ip) - 1] = 0;
    n->port = port;
    strncpy(n->id, id, 2);
    n->id[2] = 0;

    send_line(fd, "NEIGHBOR %s", st->id);
    printf("ADD EDGE ok: %s (%s:%u)\n", id, ip, port);
    return 0;
}

void cmd_direct_add_edge(state_t *st, const char *id, const char *ip, uint16_t port) {
    connect_to_neighbor(st, id, ip, port);
}

void cmd_remove_edge(state_t *st, const char *id) {
    neighbor_t *n = find_by_id(st, id);
    if (!n) {
        printf("Aresta inexistente.\n");
        return;
    }
    close_nei(n);
    printf("REMOVE EDGE ok: %s\n", id);
}

void cmd_show_neighbors(state_t *st) {
    printf("Neighbors:\n");
    bool any = false;

    for (size_t i = 0; i < st->ncount; i++) {
        neighbor_t *n = &st->nei[i];
        if (!n->active) continue;
        any = true;
        printf("%s %s %u\n", n->id[0] ? n->id : "??", n->ip, n->port);
    }

    if (!any) printf("(sem vizinhos)\n");
}

void accept_conn(state_t *st) {
    struct sockaddr_in a;
    socklen_t alen = sizeof(a);

    int fd = accept(st->listen_fd, (struct sockaddr *)&a, &alen);
    if (fd < 0) return;

    char ip[INET_ADDRSTRLEN] = {0};
    inet_ntop(AF_INET, &a.sin_addr, ip, sizeof(ip));
    uint16_t port = ntohs(a.sin_port);

    neighbor_t *n = alloc_neighbor(st);
    if (!n) {
        close(fd);
        return;
    }

    n->fd = fd;
    n->active = true;
    n->handshake = false;
    strncpy(n->ip, ip, sizeof(n->ip) - 1);
    n->ip[sizeof(n->ip) - 1] = 0;
    n->port = port;

    send_line(fd, "NEIGHBOR %s", st->id);
}
