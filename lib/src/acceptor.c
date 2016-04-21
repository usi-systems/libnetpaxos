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

void cb_func(evutil_socket_t fd, short what, void *arg)
{
    AcceptorCtx *ctx = (AcceptorCtx *) arg;
    struct sockaddr_in remote;
    socklen_t remote_len = sizeof(remote);
    Message msg;
    int n = 0, res = 0;

    n = recvfrom(fd, &msg, sizeof(Message), 0, (struct sockaddr *) &remote, &remote_len);
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

    if (msg.msgtype == phase1a) {
        res = handle_phase1a(ctx, &msg);
    }
    else if (msg.msgtype == phase2a) {
        res = handle_phase2a(ctx, &msg);
    }

    if (res) {
        pack(&msg);
        // send to learners
        socklen_t addr_len = sizeof(*ctx->learner_addr);
        n = sendto(fd, &msg, sizeof(Message), 0, (struct sockaddr*) ctx->learner_addr, addr_len);
        if (n < 0) {
            perror("ERROR in sendto");
            return;
        }
    }
}


int start_acceptor(Config *conf, int acceptor_id) {
    AcceptorCtx *ctx = acceptor_ctx_new(*conf, acceptor_id);
    ctx->base = event_base_new();
    int server_socket = create_server_socket(conf->acceptor_port);
    addMembership(conf->acceptor_addr, server_socket);

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


    struct event *recv_ev;
    recv_ev = event_new(ctx->base, server_socket, EV_READ|EV_PERSIST, cb_func, ctx);
    event_add(recv_ev, NULL);

    struct event *evsig;
    evsig = evsignal_new(ctx->base, SIGTERM, signal_handler, ctx);
    event_add(evsig, NULL);

    // Comment the line below for valgrind check
    event_base_dispatch(ctx->base);
    event_free(recv_ev);
    event_free(evsig);
    acceptor_ctx_destroy(ctx);

    return EXIT_SUCCESS;
}