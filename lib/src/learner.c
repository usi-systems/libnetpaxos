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
    char fname[32];
    int n = snprintf(fname, sizeof fname, "learner-%d.txt", conf.node_id);
    if ( n < 0 || n >= sizeof fname )
        exit(EXIT_FAILURE);
    // ctx->fp = fopen(fname, "w+");
    for (i = 0; i < VLEN; i++) {
        ctx->iovecs[i].iov_base          = (void *)&ctx->bufs[i];
        ctx->iovecs[i].iov_len           = BUFSIZE;
        ctx->msgs[i].msg_hdr.msg_iov     = &ctx->iovecs[i];
        ctx->msgs[i].msg_hdr.msg_iovlen  = 1;
    }
    ctx->timeout.tv_sec = TIMEOUT;
    ctx->timeout.tv_sec = 0;

    return ctx;
}

void learner_ctx_destroy(LearnerCtx *ctx) {
    int i;
    // fclose(ctx->fp);
    event_base_free(ctx->base);
    for (i = 0; i < ctx->conf.maxinst; i++) {
        free(ctx->states[i]->paxosval);
        free(ctx->states[i]);
    }
    free(ctx->states);
    free(ctx);
}

void signal_handler(evutil_socket_t fd, short what, void *arg) {
    LearnerCtx *ctx = (LearnerCtx *) arg;
    if (what&EV_SIGNAL) {
        event_base_loopbreak(ctx->base);
        // disable for now
        // int i;
        // for (i = 0; i < ctx->conf.maxinst; i++) {
        //     fprintf(ctx->fp, "%s\n", ctx->states[i]->paxosval);
        // }
        // fprintf(stdout, "num_packets: %d\n", ctx->num_packets);
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
                    char *value;
                    int vsize;
                    int res = ctx->deliver(state->paxosval, ctx->app, &value, &vsize);
                    ctx->mps++;
                    ctx->num_packets++;
                    int n;
                    switch(res) {
                        case SUCCESS:
                            n = sendto(fd, "SUCCESS", 8, 0, (struct sockaddr*) &msg->client, sizeof(msg->client));
                            break;
                        case GOT_VALUE: {
                            n = sendto(fd, value, vsize, 0, (struct sockaddr*) &msg->client, sizeof(msg->client));
                            free(value);
                            break;
                        }
                        case NOT_FOUND:
                            n = sendto(fd, "NOT_FOUND", 10, 0, (struct sockaddr*) &msg->client, sizeof(msg->client));
                            break;
                    }
                    if (n < 0) {
                        perror("ERROR in sendto");
                    }
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

void cb_func(evutil_socket_t fd, short what, void *arg)
{
    LearnerCtx *ctx = (LearnerCtx *) arg;

    int retval = recvmmsg(fd, ctx->msgs, VLEN, 0, &ctx->timeout);
    if (retval < 0) {
      perror("recvmmsg()");
      exit(EXIT_FAILURE);
    }

    // printf("received %d messages\n", retval);
    int i;
    for (i = 0; i < retval; i++) {
        ctx->out_bufs[i] = ctx->bufs[i];
        unpack(&ctx->out_bufs[i]);
        if (ctx->conf.verbose) {
            // printf("client info %s:%d\n", inet_ntoa(msg.client.sin_addr), ntohs(msg.client.sin_port));
            // printf("Received %d bytes from %s:%d\n", n, inet_ntoa(remote.sin_addr),
                    // ntohs(remote.sin_port));
            print_message(&ctx->out_bufs[i]);
        }
        if (ctx->out_bufs[i].inst > ctx->conf.maxinst) {
            if (ctx->conf.verbose) {
                fprintf(stderr, "State Overflow\n");
            }
            return;
        }
        handle_accepted(ctx, &ctx->out_bufs[i], fd);
    }

}


int start_learner(Config *conf, int (*deliver_cb)(const char* req, void* arg, char **value, int *vsize), void* arg) {
    LearnerCtx *ctx = learner_ctx_new(*conf);
    ctx->app = arg;
    ctx->deliver = deliver_cb;
    int server_socket = create_server_socket(conf->learner_port);
    addMembership(conf->learner_addr, server_socket);

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

    // Comment the line below for valgrind check
    event_base_dispatch(ctx->base);

    event_free(recv_ev);
    event_free(monitor_ev);
    event_free(evsig);

    learner_ctx_destroy(ctx);
   
    return EXIT_SUCCESS;
}