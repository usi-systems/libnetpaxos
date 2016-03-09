#ifndef _PROPOSER_H_
#define _PROPOSER_H_

typedef struct ProposerCtx {
    struct event_base *base;
    struct sockaddr_in *serveraddr;
    int verbose;
    int mps;
    int cur_inst;
    int max_inst;
    int64_t avg_lat;
    int *values;
    char *buffer;
} ProposerCtx;

int start_proposer();
ProposerCtx *proposer_ctx_new(int verbose, int mps, int64_t avg_lat, int max_inst);
void proposer_ctx_destroy(ProposerCtx *st);
#endif
