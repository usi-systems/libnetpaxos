#ifndef _LEARNER_H
#define _LEARNER_H
#include <sys/types.h>
#include <event2/event.h>
#include "config.h"
#include "message.h"

typedef struct LearnerCtx {
    struct event_base *base;
    Config conf;
    int mps;
    int num_packets;
    Message *msg;
    char **values;
    FILE *fp;
} LearnerCtx;

int start_learner();
LearnerCtx *learner_ctx_new(Config conf);
void learner_ctx_destroy(LearnerCtx *st);

#endif