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
#include <pthread.h>
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
    ctx->packets_in_buf = VLEN;
    ctx->count_accepted = 0;
    ctx->states = calloc(ctx->conf.maxinst, sizeof(a_state));
    int i;
    for (i = 0; i < ctx->conf.maxinst; i++) {
        ctx->states[i] = malloc(sizeof(a_state));
        ctx->states[i]->rnd = 0;
        ctx->states[i]->vrnd = 0;
        ctx->states[i]->paxosval = malloc(PAXOS_VALUE_SIZE + 1);
        bzero(ctx->states[i]->paxosval, PAXOS_VALUE_SIZE + 1);
    }
    char fname[32];
    int n = snprintf(fname, sizeof fname, "acceptor-%d.txt", acceptor_id);
    if ( n < 0 || n >= sizeof fname )
        exit(EXIT_FAILURE);
    // ctx->fp = fopen(fname, "w+");
    memset(ctx->msgs, 0, sizeof(ctx->msgs));
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


void acceptor_ctx_destroy(AcceptorCtx *ctx) {
    int i;
    event_base_free(ctx->base);
    free(ctx->learner_addr);
    // fclose(ctx->fp);
    for (i = 0; i < ctx->conf.maxinst; i++) {
        free(ctx->states[i]->paxosval);
        free(ctx->states[i]);
    }
    free(ctx->states);

    free(ctx);
}

void signal_handler(evutil_socket_t fd, short what, void *arg) {
    AcceptorCtx *ctx = (AcceptorCtx *) arg;
    if (what&EV_SIGNAL) {
        event_base_loopbreak(ctx->base);
        // int i;
        // for (i = 0; i < ctx->conf.maxinst; i++) {
        //     fprintf(ctx->fp, "%s\n", ctx->states[i]->paxosval);
        // }
        // fprintf(stdout, "num_packets: %d\n", ctx->num_packets);
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

void *on_value(void *arg)
{
    AcceptorCtx *ctx = (AcceptorCtx *) arg;
    while(1) {
        int retval = recvmmsg(ctx->sock, ctx->msgs, VLEN, 0, NULL);
        if (retval < 0) {
          perror("recvmmsg()");
          exit(EXIT_FAILURE);
        }

        int i;
        for (i = 0; i < retval; i++) {
            ctx->out_bufs[i] = ctx->bufs[i];
            unpack(&ctx->out_bufs[i]);
            if (ctx->conf.verbose) {
                printf("Received %d messages\n", retval);
                print_message(&ctx->out_bufs[i]);
            }
            if (ctx->out_bufs[i].inst > ctx->conf.maxinst) {
                if (ctx->conf.verbose) {
                    fprintf(stderr, "State Overflow\n");
                }
                return;
            }
            int res;
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
    addMembership(conf->acceptor_addr, server_socket);
    ctx->sock = server_socket;

    struct hostent *learner;
    int learnerlen;
    ctx->learner_addr = malloc( sizeof (struct sockaddr_in) );
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

    pthread_t recv_th;
    if(pthread_create(&recv_th, NULL, on_value, ctx)) {
        fprintf(stderr, "Error creating thread\n");
        exit(EXIT_FAILURE);
    }

    struct event *evsig;
    evsig = evsignal_new(ctx->base, SIGTERM, signal_handler, ctx);
    event_add(evsig, NULL);

    // Comment the line below for valgrind check
    event_base_dispatch(ctx->base);
    event_free(evsig);

    if(pthread_join(recv_th, NULL)) {
        fprintf(stderr, "Error joining thread\n");
        exit(EXIT_FAILURE);
    }

    acceptor_ctx_destroy(ctx);

    return EXIT_SUCCESS;
}