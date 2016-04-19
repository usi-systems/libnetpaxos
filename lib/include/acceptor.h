#ifndef _ACCEPTOR_H
#define _ACCEPTOR_H
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
    struct event_base *base;
    Config conf;
    Message *msg;
    a_state* *states;
    FILE *fp;
    struct sockaddr_in *learner_addr;
} AcceptorCtx;

int start_acceptor(Config *conf, int acceptor_id);

#endif