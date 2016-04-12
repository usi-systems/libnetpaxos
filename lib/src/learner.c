#include <stdio.h>
#include <event2/event.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <event2/event.h>
#include <string.h>
#include <strings.h>
#include <inttypes.h>
#include <signal.h>
#include <stdlib.h>
#include "message.h"
#include "learner.h"
#include "netpaxos_utils.h"
#include "config.h"


int create_server_socket(int port);
LearnerCtx *learner_ctx_new(Config conf);
void learner_ctx_destroy(LearnerCtx *st);

/* Here's a callback function that calls loop break */
LearnerCtx *learner_ctx_new(Config conf) {
    LearnerCtx *ctx = malloc(sizeof(LearnerCtx));
    ctx->conf = conf;
    ctx->mps = 0;
    ctx->num_packets = 0;
    ctx->msg = malloc(sizeof(Message) + 1);
    bzero(ctx->msg, sizeof(Message) + 1);
    if (ctx->msg == NULL) {
        perror("Unable to allocate memory for msg\n");
    }
    ctx->values = calloc(ctx->conf.maxinst, sizeof(char*));
    int i;
    for (i = 0; i < ctx->conf.maxinst; i++) {
        ctx->values[i] = malloc(PAXOS_VALUE_SIZE + 1);
        bzero(ctx->values[i], PAXOS_VALUE_SIZE);
    }
    char fname[32];
    int n = snprintf(fname, sizeof fname, "learner-%d.txt", conf.node_id);
    if ( n < 0 || n >= sizeof fname )
        exit(EXIT_FAILURE);
    ctx->fp = fopen(fname, "w+");

    return ctx;
}

void learner_ctx_destroy(LearnerCtx *ctx) {
    int i;
    fclose(ctx->fp);
    free(ctx->msg);
    for (i = 0; i < ctx->conf.maxinst; i++) {
        free(ctx->values[i]);
    }
    free(ctx->values);
    free(ctx);
}

void signal_handler(evutil_socket_t fd, short what, void *arg) {
    LearnerCtx *ctx = (LearnerCtx *) arg;
    if (what&EV_SIGNAL) {
        event_base_loopbreak(ctx->base);
        int i;
        for (i = 0; i < ctx->conf.maxinst; i++) {
            fprintf(ctx->fp, "%s\n", ctx->values[i]);
        }
        fprintf(stdout, "num_packets: %d\n", ctx->num_packets);
        learner_ctx_destroy(ctx);
    }
}

void monitor(evutil_socket_t fd, short what, void *arg) {
    LearnerCtx *ctx = (LearnerCtx *) arg;
    if ( ctx->mps ) {
        fprintf(stdout, "%d\n", ctx->mps);
    }
    ctx->mps = 0;
}

void cb_func(evutil_socket_t fd, short what, void *arg)
{
    LearnerCtx *ctx = (LearnerCtx *) arg;
    struct sockaddr_in remote;
    socklen_t remote_len = sizeof(remote);
    int n;

    Message msg;
    n = recvfrom(fd, &msg, sizeof(msg), 0, (struct sockaddr *) &remote, &remote_len);
    if (n < 0) {perror("ERROR in recvfrom"); return; }
    unpack(ctx->msg, &msg);
    strcpy(ctx->values[ctx->msg->inst], ctx->msg->paxosval);
    ctx->deliver(ctx->msg->paxosval);
    if (ctx->conf.verbose) print_message(ctx->msg);
    if (msg.inst >= ctx->conf.maxinst) { return; }
    n = sendto(fd, &msg, sizeof(Message), 0, (struct sockaddr*) &remote, remote_len);
    if (n < 0) {perror("ERROR in sendto"); return; }
    ctx->mps++;
    ctx->num_packets++;
    if (ctx->num_packets == ctx->conf.maxinst) { raise(SIGTERM); }
}

int create_server_socket(int port) {
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("cannot create socket");
        return EXIT_FAILURE;
    }
    struct sockaddr_in serv_addr;
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        perror("ERROR on binding");
        return EXIT_FAILURE;
    }
    return sockfd;
}

int start_learner(Config *conf, void *(*deliver_cb)(void* arg)) {
    LearnerCtx *ctx = learner_ctx_new(*conf);
    ctx->base = event_base_new();
    ctx->deliver = deliver_cb;
    int server_socket = create_server_socket(conf->learner_port);
    struct timeval timeout = {1, 0};

    struct event *recv_ev, *monitor_ev, *evsig;
    recv_ev = event_new(ctx->base, server_socket, EV_READ|EV_PERSIST, cb_func, ctx);
    monitor_ev = event_new(ctx->base, -1, EV_TIMEOUT|EV_PERSIST, monitor, ctx);
    evsig = evsignal_new(ctx->base, SIGTERM, signal_handler, ctx);

    event_base_priority_init(ctx->base, 4);
    event_priority_set(evsig, 0);
    event_priority_set(monitor_ev, 1);
    event_priority_set(recv_ev, 2);
    
    event_add(recv_ev, NULL);
    event_add(monitor_ev, &timeout);
    event_add(evsig, NULL);

    event_base_dispatch(ctx->base);
    return EXIT_SUCCESS;
}