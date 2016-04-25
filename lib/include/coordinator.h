#ifndef _PROPOSER_H_
#define _PROPOSER_H_
#define _GNU_SOURCE
#include <event2/event.h>
#include <event2/bufferevent.h>
#include <time.h>
#include "config.h"
#include "message.h"

#define VLEN 10
#define BUFSIZE 200
#define TIMEOUT 1

typedef struct CoordinatorCtx {
    struct event_base *base;
    struct sockaddr_in *acceptor_addr;
    Config conf;
    int cur_inst;
    int listen_port;
    evutil_socket_t sock;
    struct mmsghdr msgs[VLEN];
    struct iovec iovecs[VLEN];
    char bufs[VLEN][BUFSIZE+1];
    char addrbufs[VLEN][BUFSIZE+1];
    struct timespec timeout;
} CoordinatorCtx;

int start_coordinator(Config *conf);

#endif
