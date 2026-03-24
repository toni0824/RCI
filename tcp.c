#include "tcp.h"

#include <arpa/inet.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
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

static void monitor_log(state_t *st, const char *fmt, ...) {
    if (!st->monitor_enabled) return;

    va_list ap;
    va_start(ap, fmt);
    vprintf(fmt, ap);
    printf("\n");
    va_end(ap);
}

static int neighbor_index(state_t *st, neighbor_t *n) {
    if (!n) return -1;
    return (int)(n - st->nei);
}

static bool neighbor_is_ready(neighbor_t *n) {
    return n && n->active && n->handshake;
}

static route_entry_t *find_route(state_t *st, const char dest[3]) {
    for (size_t i = 0; i < st->route_count; i++) {
        if (st->routes[i].present && strncmp(st->routes[i].dest, dest, 2) == 0) return &st->routes[i];
    }
    return NULL;
}

static route_entry_t *ensure_route(state_t *st, const char dest[3]) {
    route_entry_t *rt = find_route(st, dest);
    if (rt) return rt;
    if (st->route_count >= MAX_DEST) return NULL;

    rt = &st->routes[st->route_count++];
    memset(rt, 0, sizeof(*rt));
    strncpy(rt->dest, dest, 2);
    rt->dest[2] = 0;
    rt->present = true;
    rt->distance = ROUTE_INF;
    for (size_t i = 0; i < MAX_NEI; i++) rt->advertised[i] = ROUTE_INF;
    return rt;
}

static void send_route_to_one(state_t *st, neighbor_t *n, route_entry_t *rt) {
    if (!neighbor_is_ready(n) || !rt || !rt->present) return;
    int adv = rt->valid ? rt->distance : ROUTE_INF;

    /* Poison reverse: do not advertise back to the current successor a
       usable route that depends on that same neighbor. */
    if (rt->valid && rt->successor[0] && strncmp(rt->successor, n->id, 2) == 0) {
        adv = ROUTE_INF;
    }

    send_line(n->fd, "ROUTE %s %d", rt->dest, adv);
    monitor_log(st, "-> ROUTE %s %d via %s", rt->dest, adv, n->id);
}

static void send_all_routes_to_neighbor(state_t *st, neighbor_t *n) {
    for (size_t i = 0; i < st->route_count; i++) {
        if (!st->routes[i].present) continue;
        if (st->routes[i].state != ROUTE_STATE_EXPEDITION) continue;
        send_route_to_one(st, n, &st->routes[i]);
    }
}

static void send_route_to_all(state_t *st, route_entry_t *rt, neighbor_t *exclude) {
    if (!rt || !rt->present || rt->state != ROUTE_STATE_EXPEDITION) return;
    for (size_t i = 0; i < st->ncount; i++) {
        neighbor_t *n = &st->nei[i];
        if (!neighbor_is_ready(n)) continue;
        if (exclude && n == exclude) continue;
        send_route_to_one(st, n, rt);
    }
}

static void send_coord_to_all(state_t *st, route_entry_t *rt, neighbor_t *exclude) {
    if (!rt || !rt->present) return;
    for (size_t i = 0; i < st->ncount; i++) {
        neighbor_t *n = &st->nei[i];
        if (!neighbor_is_ready(n)) continue;
        if (exclude && n == exclude) continue;
        send_line(n->fd, "COORD %s", rt->dest);
        monitor_log(st, "-> COORD %s via %s", rt->dest, n->id);
    }
}

static bool coord_wait_finished(state_t *st, route_entry_t *rt) {
    for (size_t i = 0; i < st->ncount; i++) {
        neighbor_t *n = &st->nei[i];
        if (!neighbor_is_ready(n)) continue;
        if (rt->waiting[i]) return false;
    }
    return true;
}

static void finish_coordination(state_t *st, route_entry_t *rt) {
    char trigger_id[3] = {0};
    neighbor_t *trigger = NULL;

    if (rt->trigger_neighbor[0]) {
        strncpy(trigger_id, rt->trigger_neighbor, 2);
        trigger_id[2] = 0;
        trigger = find_by_id(st, trigger_id);
    }

    rt->state = ROUTE_STATE_EXPEDITION;
    rt->trigger_neighbor[0] = 0;
    for (size_t i = 0; i < MAX_NEI; i++) rt->waiting[i] = false;

    send_route_to_all(st, rt, NULL);

    if (trigger && neighbor_is_ready(trigger)) {
        send_line(trigger->fd, "UNCOORD %s", rt->dest);
        monitor_log(st, "-> UNCOORD %s via %s", rt->dest, trigger->id);
    }
}

static void maybe_finish_coordination(state_t *st, route_entry_t *rt) {
    if (!rt || rt->state != ROUTE_STATE_COORDINATION) return;
    if (!coord_wait_finished(st, rt)) return;
    finish_coordination(st, rt);
}

static void begin_coordination(state_t *st, route_entry_t *rt, const char *trigger_id, neighbor_t *exclude) {
    rt->state = ROUTE_STATE_COORDINATION;
    if (trigger_id && trigger_id[0]) {
        strncpy(rt->trigger_neighbor, trigger_id, 2);
        rt->trigger_neighbor[2] = 0;
    } else {
        rt->trigger_neighbor[0] = 0;
    }

    for (size_t i = 0; i < MAX_NEI; i++) rt->waiting[i] = false;
    for (size_t i = 0; i < st->ncount; i++) {
        neighbor_t *n = &st->nei[i];
        if (!neighbor_is_ready(n)) continue;
        if (exclude && n == exclude) continue;
        rt->waiting[i] = true;
    }

    send_coord_to_all(st, rt, exclude);
    maybe_finish_coordination(st, rt);
}

static bool route_update_from_advertisements(state_t *st, route_entry_t *rt) {
    int best_distance = ROUTE_INF;
    char best_succ[3] = "";
    bool best_valid = false;

    if (st->joined && strncmp(rt->dest, st->id, 2) == 0) {
        best_distance = 0;
        best_valid = true;
    }

    for (size_t i = 0; i < st->ncount; i++) {
        neighbor_t *n = &st->nei[i];
        if (!n->active || !n->handshake) continue;
        if (rt->advertised[i] >= ROUTE_INF) continue;

        int candidate = rt->advertised[i] + 1;
        if (!best_valid || candidate < best_distance) {
            best_distance = candidate;
            best_valid = true;
            strncpy(best_succ, n->id, 2);
            best_succ[2] = 0;
        }
    }

    bool changed = false;
    if (best_valid != rt->valid) changed = true;
    if (best_distance != rt->distance) changed = true;
    if (strncmp(rt->successor, best_succ, 2) != 0) changed = true;

    rt->valid = best_valid;
    rt->distance = best_valid ? best_distance : ROUTE_INF;
    strncpy(rt->successor, best_succ, sizeof(rt->successor) - 1);
    rt->successor[sizeof(rt->successor) - 1] = 0;
    return changed;
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
    char work[MAX_LINE];
    strncpy(work, line, sizeof(work) - 1);
    work[sizeof(work) - 1] = 0;

    char *t[8];
    int argc = split_local(work, t, 8);
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
        notify_neighbor_confirmed(st, n);
        return;
    }

    if (strcasecmp(t[0], "ROUTE") == 0 && argc >= 3) {
        route_entry_t *rt = ensure_route(st, t[1]);
        int idx = neighbor_index(st, n);
        if (!rt || idx < 0) return;

        int advertised = atoi(t[2]);
        rt->advertised[idx] = advertised;
        monitor_log(st, "<- ROUTE %s %d de %s", t[1], advertised, n->id);
        if (route_update_from_advertisements(st, rt)) {
            if (rt->state == ROUTE_STATE_EXPEDITION) {
                send_route_to_all(st, rt, n);
            }
        }
        return;
    }

    if (strcasecmp(t[0], "COORD") == 0 && argc >= 2) {
        route_entry_t *rt = ensure_route(st, t[1]);
        monitor_log(st, "<- COORD %s de %s", t[1], n->id);
        if (!rt) return;

        if (rt->state == ROUTE_STATE_COORDINATION) {
            send_line(n->fd, "UNCOORD %s", rt->dest);
            monitor_log(st, "-> UNCOORD %s via %s", rt->dest, n->id);
            return;
        }

        if (!rt->valid || strncmp(rt->successor, n->id, 2) != 0) {
            if (rt->valid) send_route_to_one(st, n, rt);
            send_line(n->fd, "UNCOORD %s", rt->dest);
            monitor_log(st, "-> UNCOORD %s via %s", rt->dest, n->id);
            return;
        }

        begin_coordination(st, rt, n->id, NULL);
        return;
    }

    if (strcasecmp(t[0], "UNCOORD") == 0 && argc >= 2) {
        route_entry_t *rt = ensure_route(st, t[1]);
        monitor_log(st, "<- UNCOORD %s de %s", t[1], n->id);
        if (!rt) return;

        int idx = neighbor_index(st, n);
        if (idx >= 0) rt->waiting[idx] = false;
        maybe_finish_coordination(st, rt);
        return;
    }

    if (strcasecmp(t[0], "CHAT") == 0 && argc >= 4) {
        char origin[3] = {0};
        char dest[3] = {0};
        strncpy(origin, t[1], 2);
        strncpy(dest, t[2], 2);
        const char *payload = strstr(line, t[3]);
        if (!payload) payload = "";

        if (strncmp(dest, st->id, 2) == 0) {
            printf("CHAT %s: %s\n", origin, payload);
            return;
        }

        route_entry_t *rt = find_route(st, dest);
        if (!rt || !rt->valid) {
            printf("Sem rota para %s.\n", dest);
            return;
        }

        neighbor_t *next = find_by_id(st, rt->successor);
        if (!neighbor_is_ready(next)) {
            printf("Vizinho de expedição indisponível para %s.\n", dest);
            return;
        }

        send_line(next->fd, "CHAT %s %s %s", origin, dest, payload);
        monitor_log(st, "-> CHAT %s %s via %s", origin, dest, next->id);
    }
}

void pump_neighbor(state_t *st, neighbor_t *n) {
    char tmp[256];
    ssize_t r = recv(n->fd, tmp, sizeof(tmp), 0);

    if (r <= 0) {
        printf("Vizinho %s desligou.\n", n->id[0] ? n->id : "??");
        notify_neighbor_closed(st, n);
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
    notify_neighbor_closed(st, n);
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

void cmd_announce(state_t *st) {
    if (!st->joined) {
        printf("Node isn't registered on any network.\n");
        return;
    }

    route_entry_t *rt = ensure_route(st, st->id);
    if (!rt) {
        printf("Tabela de rotas cheia.\n");
        return;
    }

    rt->valid = true;
    rt->distance = 0;
    rt->state = ROUTE_STATE_EXPEDITION;
    rt->successor[0] = 0;
    rt->trigger_neighbor[0] = 0;
    for (size_t i = 0; i < MAX_NEI; i++) {
        rt->advertised[i] = ROUTE_INF;
        rt->waiting[i] = false;
    }
    printf("ANNOUNCE ok: %s\n", st->id);
    send_route_to_all(st, rt, NULL);
}

void cmd_show_routing(state_t *st, const char *dest) {
    route_entry_t *rt = find_route(st, dest);
    if (!rt) {
        printf("Sem entrada de encaminhamento para %s.\n", dest);
        return;
    }

    if (rt->valid) {
        printf("ROUTING %s state=%s distance=%d next=%s\n",
               dest,
               rt->state == ROUTE_STATE_COORDINATION ? "coord" : "exp",
               rt->distance,
               rt->successor[0] ? rt->successor : "-");
    } else {
        printf("ROUTING %s state=%s distance=INF next=-\n",
               dest,
               rt->state == ROUTE_STATE_COORDINATION ? "coord" : "exp");
    }
}

void cmd_message(state_t *st, const char *dest, const char *text) {
    if (!st->joined) {
        printf("Node isn't registered on any network.\n");
        return;
    }

    if (strncmp(dest, st->id, 2) == 0) {
        printf("CHAT %s: %s\n", st->id, text);
        return;
    }

    route_entry_t *rt = find_route(st, dest);
    if (!rt || !rt->valid || !rt->successor[0]) {
        printf("Sem rota para %s.\n", dest);
        return;
    }

    neighbor_t *n = find_by_id(st, rt->successor);
    if (!neighbor_is_ready(n)) {
        printf("Vizinho indisponível para %s.\n", dest);
        return;
    }

    send_line(n->fd, "CHAT %s %s %s", st->id, dest, text);
    printf("MESSAGE ok: %s -> %s\n", st->id, dest);
}

void cmd_monitor(state_t *st, bool enabled) {
    st->monitor_enabled = enabled;
    printf("Monitor %s.\n", enabled ? "on" : "off");
}

void notify_neighbor_confirmed(state_t *st, neighbor_t *n) {
    int idx = neighbor_index(st, n);
    if (idx >= 0) {
        for (size_t i = 0; i < st->route_count; i++) {
            route_entry_t *rt = &st->routes[i];
            if (!rt->present) continue;
            if (rt->state == ROUTE_STATE_COORDINATION) {
                rt->waiting[idx] = false;
                maybe_finish_coordination(st, rt);
            }
        }
    }

    send_all_routes_to_neighbor(st, n);
}

void notify_neighbor_closed(state_t *st, neighbor_t *n) {
    if (!n || !n->active) return;

    int idx = neighbor_index(st, n);
    for (size_t i = 0; i < st->route_count; i++) {
        route_entry_t *rt = &st->routes[i];
        bool changed = false;

        if (!rt->present) continue;
        if (idx >= 0) {
            rt->advertised[idx] = ROUTE_INF;
            rt->waiting[idx] = false;
        }

        if (rt->state == ROUTE_STATE_EXPEDITION && strncmp(rt->successor, n->id, 2) == 0) {
            begin_coordination(st, rt, NULL, n);
        }

        changed = route_update_from_advertisements(st, rt);
        if (rt->state == ROUTE_STATE_EXPEDITION && changed) {
            send_route_to_all(st, rt, NULL);
        }
        maybe_finish_coordination(st, rt);
    }
}
