#ifndef _ACCEPTOR_H
#define _ACCEPTOR_H
#define _GNU_SOURCE
#include <sys/types.h>
#include <event2/event.h>
#include "config.h"
#include "message.h"

typedef struct a_state {
    int rnd;
    int vrnd;
    char* paxosval;
} a_state;


typedef struct AcceptorCtx {
    int acceptor_id;
    int packets_in_buf;
    int count_accepted;
    struct event_base *base;
    Config conf;
    a_state* *states;
    FILE *fp;
    struct sockaddr_in *learner_addr;
    struct mmsghdr msgs[VLEN];
    struct iovec iovecs[VLEN];
    Message bufs[VLEN];
    struct mmsghdr out_msgs[VLEN];
    struct iovec out_iovecs[VLEN];
    Message out_bufs[VLEN];
    struct timespec timeout;
} AcceptorCtx;

int start_acceptor(Config *conf, int acceptor_id);

#endif