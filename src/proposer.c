#include <stdio.h>
#include <event2/event.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <event2/event.h>
#include <string.h>
#include <strings.h>
#include <inttypes.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include "proposer.h"
#include "message.h"
#include "netpaxos_utils.h"

#define LEARNER_PORT 34952
#define BUFSIZE 1470

ProposerCtx *proposer_ctx_new(int verbose, int mps, int64_t avg_lat, int max_inst) {
    ProposerCtx *ctx = malloc(sizeof(ProposerCtx));
    ctx->verbose = verbose;
    ctx->mps = mps;
    ctx->avg_lat = avg_lat;
    ctx->max_inst = max_inst;
    ctx->values = malloc(max_inst * sizeof(int));
    return ctx;
}

void proposer_ctx_destroy(ProposerCtx *st) {
    free(st->values);
    free(st);
}


void proposer_signal_handler(evutil_socket_t fd, short what, void *arg) {
    ProposerCtx *ctx = (ProposerCtx *) arg;
    if (what&EV_SIGNAL) {
        event_base_loopbreak(ctx->base);
        FILE *fp = fopen("proposer.txt", "w+");
        int i;
        for (i = 0; i < ctx->max_inst; i++) {
            fprintf(fp, "%d\n", ctx->values[i]);
        }
        fclose(fp);
        proposer_ctx_destroy(ctx);
    }
}


void perf_cb(evutil_socket_t fd, short what, void *arg)
{
    ProposerCtx *ctx = (ProposerCtx *) arg;
    if (ctx->avg_lat > 0) {
        printf("%d,%.2f\n", ctx->mps, ((double) ctx->avg_lat) / ctx->mps);
        ctx->mps = 0;
        ctx->avg_lat = 0;
    }
}

void recv_cb(evutil_socket_t fd, short what, void *arg)
{
    ProposerCtx *ctx = (ProposerCtx *) arg;
    Message msg;
    struct sockaddr_in remote;
    socklen_t remote_len = sizeof(remote);
    int n = recvfrom(fd, &msg, sizeof(msg), 0, (struct sockaddr *) &remote, &remote_len);
    if (n < 0)
      perror("ERROR in recvfrom");

    unpack(&msg);

    if (ctx->verbose) {
        char buf[BUFSIZE];
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
}


void send_cb(evutil_socket_t fd, short what, void *arg)
{
    ProposerCtx *ctx = (ProposerCtx *) arg;
    socklen_t serverlen = sizeof(*ctx->serveraddr);
    Message msg;
    msg.inst = ctx->max_inst;
    msg.rnd = 1;
    msg.vrnd = 00;
    msg.acpid = 1;
    msg.mstype = 3;
    msg.valsize = 0x04;
    msg.value = 0x01 + ctx->max_inst;
    ctx->values[ctx->max_inst] = msg.value;

    gettimeofday(&msg.ts, NULL);

    size_t msglen = sizeof(msg);
    char *buf = malloc(msglen);
    pack(&msg, buf);
    int n = sendto(fd, buf, msglen, 0, (struct sockaddr*) ctx->serveraddr, serverlen);
    if (n < 0)
        perror("ERROR in sendto");
    ctx->max_inst++;
}

int start_proposer(char* hostname, int duration, int verbose) {
    ProposerCtx *ctx = proposer_ctx_new(verbose, 0, 0.0, 65536);
    ctx->base = event_base_new();
    ctx->max_inst = 0;
    struct hostent *server;
    int serverlen;
    ctx->serveraddr = malloc( sizeof (struct sockaddr_in) );

    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        perror("cannot create socket");
    }

    server = gethostbyname(hostname);
    if (server == NULL) {
        fprintf(stderr, "ERROR, no such host as %s\n", hostname);
        return -1;
    }

    /* build the server's Internet address */
    bzero((char *) ctx->serveraddr, sizeof(struct sockaddr_in));
    ctx->serveraddr->sin_family = AF_INET;
    bcopy((char *)server->h_addr,
      (char *)&(ctx->serveraddr->sin_addr.s_addr), server->h_length);
    ctx->serveraddr->sin_port = htons(LEARNER_PORT);
    struct event *ev_recv, *ev_send, *ev_perf;
    struct timeval timeout = {0, duration};
    struct timeval perf_tm = {1, 0};
    ev_recv = event_new(ctx->base, fd, EV_READ|EV_PERSIST, recv_cb, ctx);
    ev_send = event_new(ctx->base, fd, EV_TIMEOUT|EV_PERSIST, send_cb, ctx);
    ev_perf = event_new(ctx->base, -1, EV_TIMEOUT|EV_PERSIST, perf_cb, ctx);

    event_add(ev_recv, NULL);
    event_add(ev_send, &timeout);
    event_add(ev_perf, &perf_tm);
    /* Signal event to terminate event loop */
    struct event *evsig;
    evsig = evsignal_new(ctx->base, SIGTERM, proposer_signal_handler, ctx);
    event_add(evsig, NULL);

    event_base_dispatch(ctx->base);
    close(fd);
    return 0;
}