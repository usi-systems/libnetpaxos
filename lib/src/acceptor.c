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
#include "acceptor.h"
#include "netpaxos_utils.h"
#include "config.h"


AcceptorCtx *acceptor_ctx_new(Config conf, int acceptor_id);
void acceptor_ctx_destroy(AcceptorCtx *st);

/* Here's a callback function that calls loop break */
AcceptorCtx *acceptor_ctx_new(Config conf, int acceptor_id) {
    AcceptorCtx *ctx = malloc(sizeof(AcceptorCtx));
    ctx->conf = conf;
    ctx->acceptor_id = acceptor_id;
    ctx->count_accepted = 0;
    ctx->learner_addr = malloc( sizeof(struct sockaddr_in) );
    ctx->states = calloc(ctx->conf.maxinst, sizeof(a_state));
    int i;
    for (i = 0; i < ctx->conf.maxinst; i++) {
        ctx->states[i] = malloc(sizeof(a_state));
        ctx->states[i]->rnd = 0;
        ctx->states[i]->vrnd = 0;
        ctx->states[i]->paxosval = malloc(PAXOS_VALUE_SIZE + 1);
        bzero(ctx->states[i]->paxosval, PAXOS_VALUE_SIZE + 1);
    }
    ctx->msgs = calloc(ctx->conf.vlen, sizeof(struct mmsghdr));
    ctx->iovecs = calloc(ctx->conf.vlen, sizeof(struct iovec));
    ctx->out_msgs = calloc(ctx->conf.vlen, sizeof(struct mmsghdr));
    ctx->out_iovecs = calloc(ctx->conf.vlen, sizeof(struct iovec));
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


void acceptor_ctx_destroy(AcceptorCtx *ctx) {
    int i;
    event_base_free(ctx->base);
    free(ctx->learner_addr);
    for (i = 0; i < ctx->conf.maxinst; i++) {
        free(ctx->states[i]->paxosval);
        free(ctx->states[i]);
    }
    free(ctx->states);
    free(ctx->msgs);
    free(ctx->iovecs);
    free(ctx->out_msgs);
    free(ctx->out_iovecs);
    free(ctx->out_bufs);
    free(ctx->bufs);
    free(ctx);
}

void signal_handler(evutil_socket_t fd, short what, void *arg) {
    AcceptorCtx *ctx = (AcceptorCtx *) arg;
    if (what&EV_SIGNAL) {
        printf("Stop acceptor\n");
        event_base_loopbreak(ctx->base);
    }
}


int handle_phase1a(AcceptorCtx *ctx, Message *msg) {
    a_state *state = ctx->states[msg->inst];
    if (msg->rnd > state->rnd) {
        state->rnd = msg->rnd;
        msg->vrnd = state->rnd;
        strcpy(msg->paxosval, state->paxosval);
        msg->acptid = ctx->acceptor_id;
        msg->msgtype = phase1b;
        return 1;
    }
    return 0;
}

int handle_phase2a(AcceptorCtx *ctx, Message *msg) {
    a_state *state = ctx->states[msg->inst];
    if (msg->rnd >= state->rnd) {
        state->rnd = msg->rnd;
        state->vrnd = msg->rnd;
        strcpy(state->paxosval, msg->paxosval);
        msg->acptid = ctx->acceptor_id;
        msg->msgtype = phase2b;
        return 1;
    }
    return 0;
}

void on_value(evutil_socket_t fd, short what, void *arg) {
    AcceptorCtx *ctx = (AcceptorCtx *) arg;
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
                printf("Received %d messages\n", retval);
                print_message(&ctx->out_bufs[i]);
            }
            if (ctx->out_bufs[i].inst >= (unsigned int) ctx->conf.maxinst) {
                if (ctx->conf.verbose) {
                    fprintf(stderr, "Reach Max inst\n");
                }
                raise(SIGTERM);
                return;
            }
            int res = 0;
            if (ctx->out_bufs[i].msgtype == phase1a) {
                res = handle_phase1a(ctx, &ctx->out_bufs[i]);
            }
            else if (ctx->out_bufs[i].msgtype == phase2a) {
                res = handle_phase2a(ctx, &ctx->out_bufs[i]);
            }

            if (res) {
                pack(&ctx->out_bufs[i]);
                ctx->out_iovecs[ctx->count_accepted].iov_base         = (void*)&ctx->out_bufs[ctx->count_accepted];
                ctx->out_iovecs[ctx->count_accepted].iov_len          = sizeof(Message);
                ctx->out_msgs[ctx->count_accepted].msg_hdr.msg_name    = (void *)ctx->learner_addr;
                ctx->out_msgs[ctx->count_accepted].msg_hdr.msg_namelen = sizeof(struct sockaddr_in);
                ctx->out_msgs[ctx->count_accepted].msg_hdr.msg_iov    = &ctx->out_iovecs[ctx->count_accepted];
                ctx->out_msgs[ctx->count_accepted].msg_hdr.msg_iovlen = 1; 
                ctx->count_accepted++;
            }
        }
    }
    if (ctx->count_accepted > 0) {
        int r = sendmmsg(ctx->sock, ctx->out_msgs, ctx->count_accepted, 0);
        if (r < 0) {
            perror("sendmmsg()");
            exit(EXIT_FAILURE);
        }
        ctx->count_accepted = 0;
        if (ctx->conf.verbose) {
            printf("Send %d messages\n", r);
        }
    }
}


int start_acceptor(Config *conf, int acceptor_id) {
    AcceptorCtx *ctx = acceptor_ctx_new(*conf, acceptor_id);
    ctx->base = event_base_new();
    int server_socket = create_server_socket(conf->acceptor_port);
    if (net_ip__is_multicast_ip(conf->acceptor_addr)) {
        addMembership(conf->acceptor_addr, server_socket);
    }
    ctx->sock = server_socket;

    struct hostent *learner;
    learner = gethostbyname(conf->learner_addr);
    if (learner == NULL) {
        fprintf(stderr, "ERROR, no such host as %s\n", conf->learner_addr);
        return EXIT_FAILURE;
    }

    /* build the learner's Internet address */
    bzero((char *) ctx->learner_addr, sizeof(struct sockaddr_in));
    ctx->learner_addr->sin_family = AF_INET;
    bcopy((char *)learner->h_addr,
      (char *)&(ctx->learner_addr->sin_addr.s_addr), learner->h_length);
    ctx->learner_addr->sin_port = htons(conf->learner_port);

    struct event *ev_recv;
    ev_recv = event_new(ctx->base, ctx->sock, EV_READ|EV_PERSIST, on_value, ctx);
    event_add(ev_recv, NULL);

    struct event *evs_igterm;
    evs_igterm = evsignal_new(ctx->base, SIGTERM, signal_handler, ctx);
    event_add(evs_igterm, NULL);

    struct event *ev_sigint;
    ev_sigint = evsignal_new(ctx->base, SIGINT, signal_handler, ctx);
    event_add(ev_sigint, NULL);


    // Comment the line below for valgrind check
    event_base_dispatch(ctx->base);
    event_free(evs_igterm);
    event_free(ev_sigint);
    acceptor_ctx_destroy(ctx);

    return EXIT_SUCCESS;
}