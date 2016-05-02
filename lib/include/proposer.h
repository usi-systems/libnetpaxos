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
    struct sockaddr_in *coordinator;
    struct sockaddr_in *mine;
    struct event_base *base;
    struct Config conf;
    void *app_ctx;
    deliver_fn deliver;
    struct event *ev_sigint, *ev_sigterm, *ev_recv;
};
void submit(struct proposer_state *state, char* msg, int msg_size);
void set_application_ctx(struct proposer_state *state, void *arg);
void register_callback(struct proposer_state *state, deliver_fn res_cb);
struct proposer_state *make_proposer(char *config_file, char* interface);
void free_proposer(struct proposer_state *state);
#endif
