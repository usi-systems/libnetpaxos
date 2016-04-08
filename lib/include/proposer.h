#ifndef _PROPOSER_H_
#define _PROPOSER_H_
#include <event2/event.h>
#include <event2/bufferevent.h>
#include "config.h"
#include "message.h"

typedef struct ProposerCtx {
    struct event_base *base;
    struct sockaddr_in *serveraddr;
    Config conf;
    int mps;
    int cur_inst;
    int acked_packets;
    double avg_lat;
    evutil_socket_t client_sock;
    evutil_socket_t learner_sock;
    char **values;
    Message *msg;
    char *buf;
    char *padding;
    FILE *fp;
    void *(*result_cb)(void* arg);
    struct bufferevent *bev;
} ProposerCtx;

int start_proposer(Config *conf, void *(*result_cb)(void* arg));
ProposerCtx *proposer_ctx_new(Config conf);
void proposer_ctx_destroy(ProposerCtx *st);
void submit(void *arg);
void propose_value(ProposerCtx *ctx, void *arg);
#endif
