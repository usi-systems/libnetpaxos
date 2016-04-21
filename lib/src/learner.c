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

    if (msg->rnd == state->rnd) {
        int mask = 1 << msg->acptid;
        int exist = state->from & mask;

        if (!exist) {
            state->from = state->from | mask;
            state->count++;
            strcpy(state->paxosval, msg->paxosval);
            // printf("instance: %d - count %d\n", msg->inst, state->count);
            if (state->count == ctx->maj) { // Chosen value
                state->finished = 1;        // Marked values has been chosen
                // printf("deliver %d\n", msg->inst);
                char *res = ctx->deliver(state->paxosval, ctx->app);
                ctx->mps++;
                ctx->num_packets++;
                if (!res) {
                    res = strdup("SERVER ERROR");
                }
                int n = sendto(fd, res, strlen(res), 0, (struct sockaddr*) &msg->client, sizeof(msg->client));
                if (n < 0) {perror("ERROR in sendto"); return; }
                free(res);
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

void cb_func(evutil_socket_t fd, short what, void *arg)
{
    LearnerCtx *ctx = (LearnerCtx *) arg;
    struct sockaddr_in remote;
    socklen_t remote_len = sizeof(remote);
    int n;

    Message msg;

    n = recvfrom(fd, &msg, 60, 0, (struct sockaddr *) &remote, &remote_len);
    if (n < 0) {perror("ERROR in recvfrom"); return; }
    unpack(&msg);
    if (ctx->conf.verbose) {
        printf("Received %d bytes from %s:%d\n", n, inet_ntoa(remote.sin_addr),
                ntohs(remote.sin_port));
        print_message(&msg);
    }
    if (msg.inst > ctx->conf.maxinst) {
        if (ctx->conf.verbose) {
            fprintf(stderr, "State Overflow\n");
        }
        return;
    }
    handle_accepted(ctx, &msg, fd);
}


int start_learner(Config *conf, void *(*deliver_cb)(const char* value, void* arg), void* arg) {
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