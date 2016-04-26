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

typedef struct paxos_state {
    int rnd;
    int count;
    int from;
    int finished;
    char* paxosval;
} paxos_state;


typedef struct LearnerCtx {
    struct event_base *base;
    Config conf;
    int mps;
    int num_packets;
    int maj;
    int sock;
    int res_idx;
    paxos_state* *states;
    FILE *fp;
    int (*deliver)(const char* req, void* arg, char **value, int *vsize);
    void *app;
    struct mmsghdr msgs[VLEN];
    struct iovec iovecs[VLEN];
    Message bufs[VLEN];
    struct mmsghdr out_msgs[VLEN];
    struct iovec out_iovecs[VLEN];
    Message out_bufs[VLEN];
    char res_bufs[VLEN][BUFSIZE + 1];
    struct timespec timeout;
} LearnerCtx;

int start_learner(Config *conf, int (*deliver_cb)(const char* req, void* arg, char **value, int *vsize), void* arg);

#endif