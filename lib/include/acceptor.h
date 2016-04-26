#ifndef _ACCEPTOR_H
#define _ACCEPTOR_H
#define _GNU_SOURCE
#include <sys/types.h>
#include <event2/event.h>
#include <pthread.h>
#include "config.h"
#include "message.h"

typedef struct a_state {
    int rnd;
    int vrnd;
    char* paxosval;
} a_state;


typedef struct AcceptorCtx {
    int sock;
    int acceptor_id;
    int count_accepted;
    struct event_base *base;
    Config conf;
    a_state* *states;
    FILE *fp;
    struct sockaddr_in *learner_addr;
    struct mmsghdr *msgs;
    struct iovec *iovecs;
    Message *bufs;
    struct mmsghdr *out_msgs;
    struct iovec *out_iovecs;
    Message *out_bufs;
    struct timespec timeout;
    pthread_t recv_th;
} AcceptorCtx;

int start_acceptor(Config *conf, int acceptor_id);

#endif