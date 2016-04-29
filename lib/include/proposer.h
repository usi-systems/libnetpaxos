#ifndef _PROPOSER_H_
#define _PROPOSER_H_
#define _GNU_SOURCE
#include <event2/event.h>
#include <event2/bufferevent.h>
#include <time.h>
#include "config.h"
#include "message.h"

typedef int (*deliver_fn)(char *value, int vsize, void* arg);
struct proposer_state {
    int sock;
    struct sockaddr_in *acceptor;
    struct sockaddr_in *mine;
    struct event_base *base;
    struct Config conf;
    void *app_ctx;
    deliver_fn deliver;
};
void submit(char* msg, int msg_size, struct proposer_state *state, deliver_fn res_cb, void *arg);
struct proposer_state *make_proposer(char *config_file, char* interface);
void free_proposer(struct proposer_state *state);
#endif
