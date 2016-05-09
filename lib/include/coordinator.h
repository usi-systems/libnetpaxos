#ifndef _PROPOSER_H_
#define _PROPOSER_H_
#define _GNU_SOURCE
#include <event2/event.h>
#include <event2/bufferevent.h>
#include <time.h>
#include "config.h"
#include "message.h"

typedef struct CoordinatorCtx {
    struct event_base *base;
    struct sockaddr_in *dest;
    struct sockaddr_in *mine;
    Config conf;
    int cur_inst;
    int listen_port;
    int vlen;
    evutil_socket_t sock;
    Message *msg_in;
    int rawsock;
    char datagram[BUFSIZE];
} CoordinatorCtx;

int start_coordinator(Config *conf);

#endif
