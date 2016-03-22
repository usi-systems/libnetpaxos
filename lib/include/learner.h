#ifndef _LEARNER_H
#define _LEARNER_H
#include <sys/types.h>
#include <event2/event.h>
#include "config.h"

typedef struct LearnerCtx {
    struct event_base *base;
    Config conf;
    int mps;
    int num_packets;
    int *values;
    char *buffer;
} LearnerCtx;

int start_learner();
LearnerCtx *learner_ctx_new(Config conf);
void learner_ctx_destroy(LearnerCtx *st);

#endif