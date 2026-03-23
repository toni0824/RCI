#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/select.h>
#include <time.h>
#include <unistd.h>

#include "tcp.h"
#include "udp.h"

static void trim(char *s) {
    if (!s) return;
    size_t l = strlen(s);
    while (l && (s[l - 1] == '\n' || s[l - 1] == '\r')) s[--l] = 0;
}

static int split(char *line, char *tok[], int max) {
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

static int parse_first_line(char *reply, char *line1, size_t line1_sz, char **rest) {
    char *lf = strchr(reply, '\n');
    if (lf) {
        size_t n = (size_t)(lf - reply);
        if (n >= line1_sz) n = line1_sz - 1;
        memcpy(line1, reply, n);
        line1[n] = 0;
        *rest = lf + 1;
    } else {
        strncpy(line1, reply, line1_sz - 1);
        line1[line1_sz - 1] = 0;
        *rest = reply + strlen(reply);
    }
    trim(line1);
    return 0;
}

static void cmd_show_nodes(state_t *st, const char *net) {
    if (st->direct_join) {
        printf("Nodes in net %s:\n", st->net);
        printf("%s\n", st->id);
        for (size_t i = 0; i < st->ncount; i++) {
            if (st->nei[i].active && st->nei[i].handshake && st->nei[i].id[0]) {
                printf("%s\n", st->nei[i].id);
            }
        }
        return;
    }

    char reply[2048];
    char line1[MAX_LINE];
    char *rest = NULL;

    if (do_nodes_query(st, net, reply, sizeof(reply)) < 0) {
        printf("Erro ao obter nós.\n");
        return;
    }

    parse_first_line(reply, line1, sizeof(line1), &rest);

    char kw[32], tid_s[32], op_s[32], net_s[32];
    if (sscanf(line1, "%31s %31s %31s %31s", kw, tid_s, op_s, net_s) != 4 || strcmp(kw, "NODES") != 0) {
        printf("Resposta inválida do servidor.\n");
        return;
    }

    int op = atoi(op_s);
    if (op != 1) {
        printf("Erro NODES op=%d\n", op);
        return;
    }

    printf("Nodes in net %s:\n", net_s);
    char *saveptr = NULL;
    char *line = strtok_r(rest, "\n", &saveptr);
    while (line) {
        trim(line);
        if (line[0]) printf("%s\n", line);
        line = strtok_r(NULL, "\n", &saveptr);
    }
}

static void cmd_join(state_t *st, const char *net, const char *id) {
    if (st->joined) {
        printf("Já está numa rede; faça leave primeiro.\n");
        return;
    }

    if (strlen(net) != 3 || strlen(id) != 2) {
        printf("Uso: join net id\n");
        return;
    }

    char reply[2048];
    char line1[MAX_LINE];
    char *rest = NULL;

    if (do_reg_query(st, 0, net, id, reply, sizeof(reply)) < 0) {
        printf("Erro no registo.\n");
        return;
    }

    parse_first_line(reply, line1, sizeof(line1), &rest);

    char kw[32], tid_s[32], op_s[32], net_s[32], id_s[32], ip_s[64], tcp_s[32];
    int n = sscanf(line1, "%31s %31s %31s %31s %31s %63s %31s", kw, tid_s, op_s, net_s, id_s, ip_s, tcp_s);
    if (n < 5 || strcmp(kw, "REG") != 0) {
        printf("Resposta inválida do servidor.\n");
        return;
    }

    int op = atoi(op_s);
    if (op != 1) {
        printf("Erro REG op=%d\n", op);
        return;
    }

    int fd = make_listener(st->self_tcp_port);
    if (fd < 0) {
        perror("listen");
        return;
    }

    strncpy(st->net, net, 3);
    st->net[3] = 0;
    strncpy(st->id, id, 2);
    st->id[2] = 0;
    st->listen_fd = fd;
    st->joined = true;
    st->direct_join = false;
    st->route_count = 0;
    memset(st->routes, 0, sizeof(st->routes));

    printf("JOIN ok: net=%s id=%s\n", st->net, st->id);
}

static void cmd_direct_join(state_t *st, const char *net, const char *id) {
    if (st->joined) {
        printf("Já está numa rede; faça leave primeiro.\n");
        return;
    }

    if (strlen(net) != 3 || strlen(id) != 2) {
        printf("Uso: direct join net id\n");
        return;
    }

    int fd = make_listener(st->self_tcp_port);
    if (fd < 0) {
        perror("listen");
        return;
    }

    strncpy(st->net, net, 3);
    st->net[3] = 0;
    strncpy(st->id, id, 2);
    st->id[2] = 0;
    st->listen_fd = fd;
    st->joined = true;
    st->direct_join = true;
    st->route_count = 0;
    memset(st->routes, 0, sizeof(st->routes));

    printf("JOIN ok: net=%s id=%s\n", st->net, st->id);
}

static void cmd_leave(state_t *st) {
    if (!st->joined) {
        printf("Node isn't registered on any network.\n");
        return;
    }

    char old_net[4], old_id[3];
    strncpy(old_net, st->net, sizeof(old_net) - 1);
    old_net[sizeof(old_net) - 1] = 0;
    strncpy(old_id, st->id, sizeof(old_id) - 1);
    old_id[sizeof(old_id) - 1] = 0;

    for (size_t i = 0; i < st->ncount; i++) {
        if (st->nei[i].active) close_nei(&st->nei[i]);
    }

    if (st->listen_fd >= 0) {
        close(st->listen_fd);
        st->listen_fd = -1;
    }

    if (!st->direct_join) {
        char reply[2048];
        if (do_reg_query(st, 3, old_net, old_id, reply, sizeof(reply)) < 0) {
            printf("Erro no cancelamento de registo.\n");
        }
    }

    st->joined = false;
    st->direct_join = false;
    st->net[0] = 0;
    st->id[0] = 0;
    st->route_count = 0;
    memset(st->routes, 0, sizeof(st->routes));

    printf("LEAVE ok.\n");
}

static void cmd_add_edge(state_t *st, const char *id) {
    if (!st->joined) {
        printf("Node isn't registered on any network.\n");
        return;
    }

    char reply[2048];
    char line1[MAX_LINE];
    char *rest = NULL;
    (void)rest;

    if (do_contact_query(st, st->net, id, reply, sizeof(reply)) < 0) {
        printf("Erro CONTACT.\n");
        return;
    }

    parse_first_line(reply, line1, sizeof(line1), &rest);

    char kw[32], tid_s[32], op_s[32], net_s[32], id_s[32], ip_s[64], tcp_s[32];
    int n = sscanf(line1, "%31s %31s %31s %31s %31s %63s %31s", kw, tid_s, op_s, net_s, id_s, ip_s, tcp_s);
    if (n < 5 || strcmp(kw, "CONTACT") != 0) {
        printf("Resposta inválida do servidor.\n");
        return;
    }

    int op = atoi(op_s);
    if (op == 2) {
        printf("Nó %s não registado.\n", id);
        return;
    }
    if (op != 1 || n < 7) {
        printf("Erro CONTACT op=%d\n", op);
        return;
    }

    cmd_direct_add_edge(st, id_s, ip_s, (uint16_t)atoi(tcp_s));
}

int main(int argc, char **argv) {
    state_t st;
    memset(&st, 0, sizeof(st));
    st.listen_fd = -1;
    st.running = true;

    srand((unsigned int)(time(NULL) ^ getpid()));

    for (size_t i = 0; i < MAX_NEI; i++) st.nei[i].fd = -1;

    if (argc != 3 && argc != 5) {
        fprintf(stderr, "uso: %s IP TCP [regIP regUDP]\n", argv[0]);
        return 1;
    }

    strncpy(st.self_ip, argv[1], sizeof(st.self_ip) - 1);
    st.self_ip[sizeof(st.self_ip) - 1] = 0;
    st.self_tcp_port = (uint16_t)atoi(argv[2]);

    if (argc == 5) {
        strncpy(st.server_ip, argv[3], sizeof(st.server_ip) - 1);
        st.server_ip[sizeof(st.server_ip) - 1] = 0;
        st.server_udp_port = (uint16_t)atoi(argv[4]);
    } else {
        strncpy(st.server_ip, "193.136.138.142", sizeof(st.server_ip) - 1);
        st.server_udp_port = 59000;
    }

    printf("OWR %s. My contact: %s:%u | Reg: %s:%u\n",
           st.self_ip, st.self_ip, st.self_tcp_port, st.server_ip, st.server_udp_port);

    char line[MAX_LINE];

    while (st.running) {
        fd_set rf;
        FD_ZERO(&rf);
        FD_SET(STDIN_FILENO, &rf);
        int maxfd = STDIN_FILENO;

        if (st.listen_fd >= 0) {
            FD_SET(st.listen_fd, &rf);
            if (st.listen_fd > maxfd) maxfd = st.listen_fd;
        }

        for (size_t i = 0; i < st.ncount; i++) {
            if (st.nei[i].active && st.nei[i].fd >= 0) {
                FD_SET(st.nei[i].fd, &rf);
                if (st.nei[i].fd > maxfd) maxfd = st.nei[i].fd;
            }
        }

        int r = select(maxfd + 1, &rf, NULL, NULL, NULL);
        if (r < 0) {
            if (errno == EINTR) continue;
            perror("select");
            break;
        }

        if (FD_ISSET(STDIN_FILENO, &rf)) {
            if (!fgets(line, sizeof(line), stdin)) {
                st.running = false;
                break;
            }

            trim(line);
            if (!line[0]) continue;

            char work[MAX_LINE];
            strncpy(work, line, sizeof(work) - 1);
            work[sizeof(work) - 1] = 0;

            char *t[8];
            int c = split(work, t, 8);
            if (c == 0) continue;

            if (strcasecmp(t[0], "exit") == 0 || strcasecmp(t[0], "x") == 0) {
                st.running = false;
            }
            else if (strcasecmp(t[0], "leave") == 0 || strcasecmp(t[0], "l") == 0) {
                cmd_leave(&st);
            }
            else if ((strcasecmp(t[0], "join") == 0 || strcasecmp(t[0], "j") == 0) && c >= 3) {
                cmd_join(&st, t[1], t[2]);
            }
            else if ((strcasecmp(t[0], "direct") == 0 || strcasecmp(t[0], "dj") == 0 ||
                      (strcasecmp(t[0], "direct") == 0 && c >= 2 && strcasecmp(t[1], "join") == 0))) {
                if (strcasecmp(t[0], "dj") == 0 && c >= 3) {
                    cmd_direct_join(&st, t[1], t[2]);
                } else if (strcasecmp(t[0], "direct") == 0 && c >= 4 && strcasecmp(t[1], "join") == 0) {
                    cmd_direct_join(&st, t[2], t[3]);
                } else {
                    printf("Uso: direct join net id\n");
                }
            }
            else if ((strcasecmp(t[0], "show") == 0 && c >= 3 && strcasecmp(t[1], "nodes") == 0) ||
                     (strcasecmp(t[0], "n") == 0 && c >= 2)) {
                if (strcasecmp(t[0], "n") == 0) cmd_show_nodes(&st, t[1]);
                else cmd_show_nodes(&st, t[2]);
            }
            else if ((strcasecmp(t[0], "show") == 0 && c >= 2 && strcasecmp(t[1], "neighbors") == 0) ||
                     strcasecmp(t[0], "sg") == 0) {
                cmd_show_neighbors(&st);
            }
            else if ((strcasecmp(t[0], "announce") == 0 || strcasecmp(t[0], "a") == 0)) {
                cmd_announce(&st);
            }
            else if ((strcasecmp(t[0], "show") == 0 && c >= 3 && strcasecmp(t[1], "routing") == 0) ||
                     (strcasecmp(t[0], "sr") == 0 && c >= 2)) {
                if (strcasecmp(t[0], "sr") == 0) cmd_show_routing(&st, t[1]);
                else cmd_show_routing(&st, t[2]);
            }
            else if ((strcasecmp(t[0], "start") == 0 && c >= 2 && strcasecmp(t[1], "monitor") == 0) ||
                     strcasecmp(t[0], "sm") == 0) {
                cmd_monitor(&st, true);
            }
            else if ((strcasecmp(t[0], "end") == 0 && c >= 2 && strcasecmp(t[1], "monitor") == 0) ||
                     strcasecmp(t[0], "em") == 0) {
                cmd_monitor(&st, false);
            }
            else if ((strcasecmp(t[0], "add") == 0 && c >= 3 && strcasecmp(t[1], "edge") == 0) ||
                     (strcasecmp(t[0], "ae") == 0 && c >= 2)) {
                if (strcasecmp(t[0], "ae") == 0) cmd_add_edge(&st, t[1]);
                else cmd_add_edge(&st, t[2]);
            }
            else if ((strcasecmp(t[0], "direct") == 0 && c >= 6 && strcasecmp(t[1], "add") == 0) ||
                     (strcasecmp(t[0], "dae") == 0 && c >= 4)) {
                if (strcasecmp(t[0], "dae") == 0) {
                    cmd_direct_add_edge(&st, t[1], t[2], (uint16_t)atoi(t[3]));
                } else if (strcasecmp(t[2], "edge") == 0) {
                    cmd_direct_add_edge(&st, t[3], t[4], (uint16_t)atoi(t[5]));
                } else {
                    printf("Uso direct add edge id ip porto\n");
                }
            }
            else if ((strcasecmp(t[0], "remove") == 0 && c >= 3 && strcasecmp(t[1], "edge") == 0) ||
                     (strcasecmp(t[0], "re") == 0 && c >= 2)) {
                if (strcasecmp(t[0], "re") == 0) cmd_remove_edge(&st, t[1]);
                else cmd_remove_edge(&st, t[2]);
            }
            else if ((strcasecmp(t[0], "message") == 0 && c >= 3) ||
                     (strcasecmp(t[0], "m") == 0 && c >= 3)) {
                char *payload = strstr(line, t[2]);
                if (!payload) payload = t[2];
                cmd_message(&st, t[1], payload);
            }
            else {
                printf("comando?\n");
            }
        }

        if (st.listen_fd >= 0 && FD_ISSET(st.listen_fd, &rf)) accept_conn(&st);

        for (size_t i = 0; i < st.ncount; i++) {
            if (st.nei[i].active && st.nei[i].fd >= 0 && FD_ISSET(st.nei[i].fd, &rf)) {
                pump_neighbor(&st, &st.nei[i]);
            }
        }
    }

    if (st.joined) cmd_leave(&st);
    
    return 0;
}
