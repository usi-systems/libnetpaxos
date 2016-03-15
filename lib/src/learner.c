#include <stdio.h>
#include <event2/event.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <event2/event.h>
#include <strings.h>
#include <inttypes.h>
#include <signal.h>
#include <stdlib.h>
#include "message.h"
#include "learner.h"
#include "netpaxos_utils.h"
#include "config.h"

/* Here's a callback function that calls loop break */
LearnerCtx *learner_ctx_new(int verbose, int max_inst, int bufsize) {
    LearnerCtx *ctx = malloc(sizeof(LearnerCtx));
    ctx->verbose = verbose;
    ctx->mps = 0;
    ctx->avg_lat = 0.0;
    ctx->max_inst = max_inst;
    ctx->num_packets = 0;
    ctx->bufsize = bufsize;
    ctx->values = malloc(max_inst * sizeof(int));
    return ctx;
}

void learner_ctx_destroy(LearnerCtx *st) {
    free(st->values);
    free(st);
}

void signal_handler(evutil_socket_t fd, short what, void *arg) {
    LearnerCtx *ctx = (LearnerCtx *) arg;
    if (what&EV_SIGNAL) {
        event_base_loopbreak(ctx->base);
        FILE *fp = fopen("learner.txt", "w+");
        int i;
        for (i = 0; i <= ctx->max_inst; i++) {
            fprintf(fp, "%d\n", ctx->values[i]);
        }
        fprintf(fp, "num_packets: %d\n", ctx->num_packets);
        fclose(fp);
        learner_ctx_destroy(ctx);
    }
}

void monitor(evutil_socket_t fd, short what, void *arg) {
    LearnerCtx *ctx = (LearnerCtx *) arg;
    if (ctx->avg_lat > 0) {
        printf("%d,%.2f\n", ctx->mps, ((double) ctx->avg_lat) / ctx->mps);
        ctx->mps = 0;
        ctx->avg_lat = 0;
    }
}

void cb_func(evutil_socket_t fd, short what, void *arg)
{
    LearnerCtx *ctx = (LearnerCtx *) arg;
    Message msg;
    struct sockaddr_in remote;
    socklen_t remote_len = sizeof(remote);
    int n = recvfrom(fd, &msg, sizeof(msg), 0, (struct sockaddr *) &remote, &remote_len);
    if (n < 0)
      perror("ERROR in recvfrom");
    char buf[ctx->bufsize];
    unpack(&msg);
    if (ctx->verbose) {
        message_to_string(msg, buf);
        printf("%s" , buf);
    }
    ctx->mps++;
    struct timeval end, result;
    gettimeofday(&end, NULL);
    if (timeval_subtract(&result, &end, &msg.ts) < 0) {
        printf("Latency is negative");
    }
    int64_t latency = (int64_t) (result.tv_sec*1000000 + result.tv_usec);
    ctx->avg_lat += latency;
    ctx->values[msg.inst] = msg.value;
    // Echo the received message
    size_t msglen = sizeof(msg);
    pack(&msg, buf);
    n = sendto(fd, buf, msglen, 0, (struct sockaddr*) &remote, remote_len);
    if (n < 0)
        perror("ERROR in sendto");
    ctx->num_packets++;

    if (msg.inst == ctx->max_inst - 1) {
        raise(SIGTERM);
    }
}

int start_learner(Config *conf) {
    LearnerCtx *ctx = learner_ctx_new(conf->verbose, conf->maxinst, conf->bufsize);
    ctx->base = event_base_new();
    event_base_priority_init(ctx->base, 4);
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        perror("cannot create socket");
    }

    struct sockaddr_in serv_addr;
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(conf->learner_port);
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
        perror("ERROR on binding");
    struct event *recv_ev, *monitor_ev, *evsig;
    recv_ev = event_new(ctx->base, fd, EV_READ|EV_PERSIST, cb_func, ctx);
    struct timeval timeout = {1, 0};
    monitor_ev = event_new(ctx->base, -1, EV_TIMEOUT|EV_PERSIST, monitor, ctx);
    evsig = evsignal_new(ctx->base, SIGTERM, signal_handler, ctx);
    event_priority_set(evsig, 0);
    event_priority_set(monitor_ev, 1);
    event_priority_set(recv_ev, 2);
    
    event_add(recv_ev, NULL);
    event_add(monitor_ev, &timeout);
    /* Signal event to terminate event loop */
    event_add(evsig, NULL);

    event_base_dispatch(ctx->base);
    return 0;
}