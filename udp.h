#ifndef UDP_H
#define UDP_H

#include "tcp.h"
#include <stddef.h>

#define UDP_TIMEOUT_SEC 3

int do_reg_query(state_t *st, int op, const char *net, const char *id, char *reply, size_t reply_sz);
int do_nodes_query(state_t *st, const char *net, char *reply, size_t reply_sz);
int do_contact_query(state_t *st, const char *net, const char *id, char *reply, size_t reply_sz);

#endif
