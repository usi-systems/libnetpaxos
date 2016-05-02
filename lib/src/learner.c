#define _GNU_SOURCE
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
#include <math.h>
#include <errno.h>
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
    ctx->base = event_base_new();
    ctx->conf = conf;
    ctx->mps = 0;
    ctx->res_idx = 0;
    ctx->num_packets = 0;
    double maj = ((double)(conf.num_acceptors + 1)) / 2;
    ctx->maj = ceil(maj);
    ctx->states = calloc(ctx->conf.maxinst, sizeof(paxos_state*));
    int i;
    for (i = 0; i < ctx->conf.maxinst; i++) {
        ctx->states[i] = malloc(sizeof(paxos_state));
        ctx->states[i]->rnd = 0;
        ctx->states[i]->from = 0;
        ctx->states[i]->count = 0;
        ctx->states[i]->finished = 0;
        ctx->states[i]->paxosval = malloc(PAXOS_VALUE_SIZE);
        bzero(ctx->states[i]->paxosval, PAXOS_VALUE_SIZE);
    }
    ctx->msgs = calloc(ctx->conf.vlen, sizeof(struct mmsghdr));
    ctx->iovecs = calloc(ctx->conf.vlen, sizeof(struct iovec));
    ctx->out_bufs = calloc(ctx->conf.vlen, sizeof(struct Message));
    ctx->bufs = calloc(ctx->conf.vlen, sizeof(struct Message));
    for (i = 0; i < ctx->conf.vlen; i++) {
        ctx->iovecs[i].iov_base          = &ctx->bufs[i];
        ctx->iovecs[i].iov_len           = sizeof(struct Message);
        ctx->msgs[i].msg_hdr.msg_iov     = &ctx->iovecs[i];
        ctx->msgs[i].msg_hdr.msg_iovlen  = 1;
    }
    return ctx;
}

void free_learner(LearnerCtx *ctx) {
    int i;
    event_free(ctx->recv_ev);
    event_free(ctx->monitor_ev);
    event_free(ctx->ev_sigterm);
    event_free(ctx->ev_sigint);
    event_base_free(ctx->base);
    for (i = 0; i < ctx->conf.maxinst; i++) {
        free(ctx->states[i]->paxosval);
        free(ctx->states[i]);
    }
    free(ctx->states);
    free(ctx->msgs);
    free(ctx->iovecs);
    free(ctx->bufs);
    free(ctx->out_bufs);
    free(ctx);
}

void signal_handler(evutil_socket_t fd, short what, void *arg) {
    LearnerCtx *ctx = (LearnerCtx *) arg;
    if (what&EV_SIGNAL) {
        printf("Stop learner\n");
        event_base_loopbreak(ctx->base);
    }
}

void monitor(evutil_socket_t fd, short what, void *arg) {
    LearnerCtx *ctx = (LearnerCtx *) arg;
    if ( ctx->mps ) {
        fprintf(stdout, "%d\n", ctx->mps);
    }
    ctx->mps = 0;
}


void handle_accepted(LearnerCtx *ctx, Message *msg, evutil_socket_t fd) {
    paxos_state *state = ctx->states[msg->inst];
    if (!state->finished) {
        if (msg->rnd == state->rnd) {
            int mask = 1 << msg->acptid;
            int exist = state->from & mask;
            if (!exist) {
                state->from = state->from | mask;
                state->count++;
                if (!state->paxosval[0]) {
                    strcpy(state->paxosval, msg->paxosval);
                }
                // printf("instance: %d - count %d\n", msg->inst, state->count);
                if (state->count == ctx->maj) { // Chosen value
                    state->finished = 1;        // Marked values has been chosen
                    // printf("deliver %d\n", msg->inst);
                    struct app_request req;
                    req.value = state->paxosval;
                    req.size = PAXOS_VALUE_SIZE;
                    req.client = &msg->client;
                    ctx->deliver(ctx, msg->inst, &req);
                    ctx->mps++;
                    ctx->num_packets++;
                }
            }
        } else if (msg->rnd > state->rnd) {
            state->rnd = msg->rnd;
            int mask = 1 << msg->acptid;
            state->from = state->from | mask;
            state->count = 1;
            strcpy(state->paxosval, msg->paxosval);
        }
    }
}


void on_value(evutil_socket_t fd, short what, void *arg)
{
    LearnerCtx *ctx = arg;
    int retval = recvmmsg(ctx->sock, ctx->msgs, ctx->conf.vlen, MSG_WAITFORONE, NULL);
    if (retval < 0) {
      perror("recvmmsg()");
      exit(EXIT_FAILURE);
    }
    else if (retval > 0) {
        int i;
        for (i = 0; i < retval; i++) {
            ctx->out_bufs[i] = ctx->bufs[i];
            unpack(&ctx->out_bufs[i]);
            if (ctx->conf.verbose) {
                printf("client info %s:%d\n",
                    inet_ntoa(ctx->out_bufs[i].client.sin_addr),
                    ntohs(ctx->out_bufs[i].client.sin_port));
                printf("received %d messages\n", retval);
                print_message(&ctx->out_bufs[i]);
            }
            if (ctx->out_bufs[i].inst > (unsigned int) ctx->conf.maxinst) {
                if (ctx->conf.verbose) {
                    fprintf(stderr, "State Overflow\n");
                }
                return;
            }
            handle_accepted(ctx, &ctx->out_bufs[i], ctx->sock);
        }
    }
}


void set_app_ctx(struct LearnerCtx *learner_ctx, void *arg) {
    learner_ctx->app = arg;
}
void register_deliver_cb(struct LearnerCtx *learner_ctx, deliver_cb deliver) {
    learner_ctx->deliver = deliver;
}

LearnerCtx* make_learner(Config *conf) {
    LearnerCtx *ctx = learner_ctx_new(*conf);
    int server_socket = create_server_socket(conf->learner_port);
    addMembership(conf->learner_addr, server_socket);
    ctx->sock = server_socket;
    ctx->timeout.tv_sec = 1;
    ctx->timeout.tv_usec = 0;
    ctx->recv_ev = event_new(ctx->base, ctx->sock, EV_READ|EV_PERSIST, on_value, ctx);
    ctx->monitor_ev = event_new(ctx->base, -1, EV_TIMEOUT|EV_PERSIST, monitor, ctx);
    ctx->ev_sigterm = evsignal_new(ctx->base, SIGTERM, signal_handler, ctx);
    ctx->ev_sigint = evsignal_new(ctx->base, SIGINT, signal_handler, ctx);
    event_base_priority_init(ctx->base, 4);
    event_priority_set(ctx->ev_sigint, 0);
    event_priority_set(ctx->ev_sigterm, 1);
    event_priority_set(ctx->monitor_ev, 2);
    event_priority_set(ctx->recv_ev, 3);
    event_add(ctx->ev_sigint, NULL);
    event_add(ctx->recv_ev, NULL);
    event_add(ctx->monitor_ev, &ctx->timeout);
    event_add(ctx->ev_sigterm, NULL);
    return ctx;
}