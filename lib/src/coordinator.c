#define _GNU_SOURCE
#include <stdio.h>
#include <event2/event.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <event2/listener.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <string.h>
#include <strings.h>
#include <inttypes.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include "message.h"
#include "netpaxos_utils.h"
#include "config.h"
#include "coordinator.h"

void propose_value(CoordinatorCtx *ctx, void *arg, int size, struct sockaddr_in client);

CoordinatorCtx *coordinator_new(Config conf) {
    CoordinatorCtx *ctx = malloc(sizeof(CoordinatorCtx));
    ctx->conf = conf;
    ctx->cur_inst = 0;
    ctx->msgs = calloc(ctx->conf.vlen, sizeof(struct mmsghdr));
    ctx->iovecs = calloc(ctx->conf.vlen, sizeof(struct iovec));
    ctx->out_msgs = calloc(ctx->conf.vlen, sizeof(struct mmsghdr));
    ctx->out_iovecs = calloc(ctx->conf.vlen, sizeof(struct iovec));
    ctx->out_bufs = calloc(ctx->conf.vlen, sizeof(struct Message));
    ctx->bufs = calloc(ctx->conf.vlen, sizeof(char*));
    ctx->addrbufs = calloc(ctx->conf.vlen, sizeof(char*));
    int i;
    for (i = 0; i < ctx->conf.vlen; i++) {
        ctx->bufs[i] = malloc(BUFSIZE + 1);
        ctx->addrbufs[i] = malloc(BUFSIZE + 1);
    }
    for (i = 0; i < ctx->conf.vlen; i++) {
        ctx->iovecs[i].iov_base          = ctx->bufs[i];
        ctx->iovecs[i].iov_len           = BUFSIZE;
        ctx->msgs[i].msg_hdr.msg_iov     = &ctx->iovecs[i];
        ctx->msgs[i].msg_hdr.msg_iovlen  = 1;
        ctx->msgs[i].msg_hdr.msg_name    = ctx->addrbufs[i];
        ctx->msgs[i].msg_hdr.msg_namelen = BUFSIZE;
    }
    return ctx;
}

void coordinator_free(CoordinatorCtx *ctx) {
    event_base_free(ctx->base);
    free(ctx->msgs);
    free(ctx->iovecs);
    free(ctx->out_msgs);
    free(ctx->out_iovecs);
    free(ctx->out_bufs);
    int i;
    for (i = 0; i < ctx->conf.vlen; i++) {
        free(ctx->bufs[i]);
        free(ctx->addrbufs[i]);
    }
    free(ctx->acceptor_addr);
    free(ctx->bufs);
    free(ctx->addrbufs);
    free(ctx);

}

void init_out_msgs(CoordinatorCtx *ctx) {
    int i;
    for (i = 0; i < ctx->conf.vlen; i++) {
        ctx->out_msgs[i].msg_hdr.msg_name    = (void *)ctx->acceptor_addr;
        ctx->out_msgs[i].msg_hdr.msg_namelen = sizeof(struct sockaddr_in);
        ctx->out_msgs[i].msg_hdr.msg_iov    = &ctx->out_iovecs[i];
        ctx->out_msgs[i].msg_hdr.msg_iovlen = 1;
    }
}


void signal_handler(evutil_socket_t fd, short what, void *arg) {
    CoordinatorCtx *ctx = arg;
    if (what&EV_SIGNAL) {
        printf("Stop Coordinator\n");
        event_base_loopbreak(ctx->base);
    }
}

void on_value(evutil_socket_t fd, short what, void *arg) {
    CoordinatorCtx *ctx = (CoordinatorCtx *) arg;
    int retval = recvmmsg(ctx->sock, ctx->msgs, ctx->conf.vlen, MSG_WAITFORONE, NULL);
    if (retval < 0) {
      perror("recvmmsg()");
    }
    else if (retval > 0) {
        int i;
        for (i = 0; i < retval; i++) {
            ctx->bufs[i][ctx->msgs[i].msg_len] = 0;
            struct sockaddr_in *client = ctx->msgs[i].msg_hdr.msg_name;
            // printf("received from %s:%d\n", inet_ntoa(client->sin_addr), ntohs(client->sin_port));
            if (ctx->cur_inst >= ctx->conf.maxinst)
                ctx->cur_inst = 0;
            initialize_message(&ctx->out_bufs[i], phase2a);
            memcpy(ctx->out_bufs[i].paxosval, ctx->bufs[i], ctx->msgs[i].msg_len - 1);
            ctx->out_bufs[i].inst = ctx->cur_inst++;
            ctx->out_bufs[i].client = *client;
            if(ctx->conf.verbose) {
                printf("Received %d messages\n", retval);
                print_message(&ctx->out_bufs[i]);
            }
            pack(&ctx->out_bufs[i]);
            ctx->out_iovecs[i].iov_base         = (void*)&ctx->out_bufs[i];
            ctx->out_iovecs[i].iov_len          = sizeof(Message);
        }
        int r = sendmmsg(ctx->sock, ctx->out_msgs, retval, 0);
        if (r <= 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
            }

            if (errno == ECONNREFUSED) {
            }
            perror("sendmmsg()");
        }
        if(ctx->conf.verbose) {
            printf("Send %d messages\n", r);
        }
    }
}



int start_coordinator(Config *conf) {
    CoordinatorCtx *ctx = coordinator_new(*conf);
    ctx->base = event_base_new();
    struct hostent *server;
    int serverlen;
    ctx->acceptor_addr = malloc( sizeof (struct sockaddr_in) );

    server = gethostbyname(conf->acceptor_addr);
    if (server == NULL) {
        fprintf(stderr, "ERROR, no such host as %s\n", conf->acceptor_addr);
        return EXIT_FAILURE;
    }

    /* build the server's Internet address */
    bzero((char *) ctx->acceptor_addr, sizeof(struct sockaddr_in));
    ctx->acceptor_addr->sin_family = AF_INET;
    bcopy((char *)server->h_addr,
      (char *)&(ctx->acceptor_addr->sin_addr.s_addr), server->h_length);
    ctx->acceptor_addr->sin_port = htons(conf->acceptor_port);

    init_out_msgs(ctx);

    int listen_socket = create_server_socket(conf->proposer_port);
    addMembership(conf->proposer_addr, listen_socket);
    ctx->sock = listen_socket;

    struct event *ev_recv;
    ev_recv = event_new(ctx->base, ctx->sock, EV_READ|EV_PERSIST, on_value, ctx);
    event_add(ev_recv, NULL);

    struct event *ev_sigterm;
    ev_sigterm = evsignal_new(ctx->base, SIGTERM, signal_handler, ctx);
    event_add(ev_sigterm, NULL);

    struct event *ev_sigint;
    ev_sigint = evsignal_new(ctx->base, SIGINT, signal_handler, ctx);
    event_add(ev_sigint, NULL);

    event_base_dispatch(ctx->base);
    event_free(ev_recv);
    event_free(ev_sigterm);
    event_free(ev_sigint);
    coordinator_free(ctx);

    close(listen_socket);
    return EXIT_SUCCESS;
}

