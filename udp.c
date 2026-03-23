#include "udp.h"

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

static int next_tid(void) {
    return rand() % 1000;
}

static int udp_request_reply(state_t *st, const char *msg, char *reply, size_t reply_sz) {
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        perror("socket udp");
        return -1;
    }

    struct timeval tv;
    tv.tv_sec = UDP_TIMEOUT_SEC;
    tv.tv_usec = 0;
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        perror("setsockopt");
        close(fd);
        return -1;
    }

    struct sockaddr_in srv;
    memset(&srv, 0, sizeof(srv));
    srv.sin_family = AF_INET;
    srv.sin_port = htons(st->server_udp_port);

    if (inet_pton(AF_INET, st->server_ip, &srv.sin_addr) != 1) {
        fprintf(stderr, "ip do servidor inválido: %s\n", st->server_ip);
        close(fd);
        return -1;
    }

    ssize_t sent = sendto(fd, msg, strlen(msg), 0, (struct sockaddr *)&srv, sizeof(srv));
    if (sent < 0) {
        perror("sendto");
        close(fd);
        return -1;
    }

    if (!reply || reply_sz == 0) {
        close(fd);
        return 0;
    }

    ssize_t n = recvfrom(fd, reply, reply_sz - 1, 0, NULL, NULL);
    if (n < 0) {
        perror("recvfrom");
        close(fd);
        return -1;
    }

    reply[n] = 0;
    close(fd);
    return 0;
}

int do_reg_query(state_t *st, int op, const char *net, const char *id, char *reply, size_t reply_sz) {
    int tid = next_tid();
    char msg[MAX_LINE];

    if (op == 0) {
        snprintf(msg, sizeof(msg), "REG %03d 0 %s %s %s %u", tid, net, id, st->self_ip, st->self_tcp_port);
    } else if (op == 3) {
        snprintf(msg, sizeof(msg), "REG %03d 3 %s %s", tid, net, id);
    } else {
        return -1;
    }

    return udp_request_reply(st, msg, reply, reply_sz);
}

int do_nodes_query(state_t *st, const char *net, char *reply, size_t reply_sz) {
    int tid = next_tid();
    char msg[MAX_LINE];
    snprintf(msg, sizeof(msg), "NODES %03d 0 %s", tid, net);
    return udp_request_reply(st, msg, reply, reply_sz);
}

int do_contact_query(state_t *st, const char *net, const char *id, char *reply, size_t reply_sz) {
    int tid = next_tid();
    char msg[MAX_LINE];
    snprintf(msg, sizeof(msg), "CONTACT %03d 0 %s %s", tid, net, id);
    return udp_request_reply(st, msg, reply, reply_sz);
}
