#ifndef _LEARNER_H
#define _LEARNER_H
#define _GNU_SOURCE
#include <sys/types.h>
#include <event2/event.h>
#include "config.h"
#include "message.h"

enum return_code {
    SUCCESS,
    GOT_VALUE,
    NOT_FOUND,
    INVALID_OP,
    FAILED
};

typedef struct LearnerCtx LearnerCtx;
typedef struct paxos_state paxos_state;
typedef struct app_request app_request;

typedef struct paxos_state {
    int rnd;
    int count;
    int from;
    int finished;
    char* paxosval;
} paxos_state;


struct app_request {
    char* value; 
    int size;
    struct sockaddr_in *client;
};

typedef int (*deliver_cb)(struct LearnerCtx *ctx, int inst, char *value, int size);

typedef struct LearnerCtx {
    struct event_base *base;
    struct event *recv_ev;
    struct event *monitor_ev;
    struct event *ev_sigterm;
    struct event *ev_sigint;
    struct timeval timeout;

    Config conf;
    int mps;
    int num_packets;
    int maj;
    int sock;
    int res_idx;
    paxos_state* *states;
    FILE *fp;
    deliver_cb deliver;
    void *app;
    struct mmsghdr *msgs;
    struct iovec *iovecs;
    Message *bufs;
    Message *out_bufs;
    struct sockaddr_in *mine;
    struct sockaddr_in *dest;
} LearnerCtx;

LearnerCtx* make_learner(Config *conf);
void set_app_ctx(struct LearnerCtx *learner_ctx, void *arg);
void register_deliver_cb(struct LearnerCtx *learner_ctx, deliver_cb deliver);
void recover(struct LearnerCtx * ctx, int instance, char * value, int size);
void free_learner(LearnerCtx *ctx);
#endif