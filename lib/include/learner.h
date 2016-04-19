#ifndef _LEARNER_H
#define _LEARNER_H
#include <sys/types.h>
#include <event2/event.h>
#include "config.h"
#include "message.h"


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
    Message *msg;
    paxos_state* *states;
    FILE *fp;
    void *(*deliver)(char* value, void* arg);
    void *app;
} LearnerCtx;

int start_learner(Config *conf, void *(*deliver_cb)(char* value, void* arg), void* arg);

#endif