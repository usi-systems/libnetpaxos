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

CoordinatorCtx *proposer_ctx_new(Config conf) {
    CoordinatorCtx *ctx = malloc(sizeof(CoordinatorCtx));
    ctx->conf = conf;
    ctx->cur_inst = 0;
    memset(ctx->msgs, 0, sizeof(ctx->msgs));
    int i;
    for (i = 0; i < VLEN; i++) {
        ctx->iovecs[i].iov_base          = ctx->bufs[i];
        ctx->iovecs[i].iov_len           = BUFSIZE;
        ctx->msgs[i].msg_hdr.msg_iov     = &ctx->iovecs[i];
        ctx->msgs[i].msg_hdr.msg_iovlen  = 1;
        ctx->msgs[i].msg_hdr.msg_name    = &ctx->addrbufs[i];
        ctx->msgs[i].msg_hdr.msg_namelen = BUFSIZE;
    }
    ctx->timeout.tv_sec = TIMEOUT;
    ctx->timeout.tv_sec = 0;
    return ctx;
}


void on_value(evutil_socket_t fd, short what, void *arg)
{
    CoordinatorCtx *ctx = (CoordinatorCtx *) arg;
    int retval = recvmmsg(fd, ctx->msgs, VLEN, 0, &ctx->timeout);
    if (retval < 0) {
      perror("recvmmsg()");
      exit(EXIT_FAILURE);
    }
    printf("%d messages received\n", retval);
    int i;
    for (i = 0; i < retval; i++) {
        ctx->bufs[i][ctx->msgs[i].msg_len] = 0;
        printf("%d %s", i+1, ctx->bufs[i]);
        struct sockaddr_in *client = ctx->msgs[i].msg_hdr.msg_name;
        printf("sizeof %d\n", ctx->msgs[i].msg_hdr.msg_namelen);

        printf("received from %s:%d\n", inet_ntoa(client->sin_addr),
                            ntohs(client->sin_port));
        propose_value(ctx, ctx->bufs[i], ctx->msgs[i].msg_len, *client);
    }
}


void propose_value(CoordinatorCtx *ctx, void *arg, int size, struct sockaddr_in client)
{
    char *v = arg;
    socklen_t serverlen = sizeof(*ctx->acceptor_addr);
    Message msg;
    initialize_message(&msg, phase2a);
    msg.client = client;
    if (ctx->cur_inst >= ctx->conf.maxinst) {
        return;
    }
    msg.inst = ctx->cur_inst;

    int maxlen = ( size < PAXOS_VALUE_SIZE ? size : PAXOS_VALUE_SIZE - 1);
    strncpy(msg.paxosval, v, maxlen);

    pack(&msg);

    int n = sendto(ctx->sock, &msg, sizeof(Message), 0, (struct sockaddr*) ctx->acceptor_addr, serverlen);
    if (n < 0) {
        perror("ERROR in sendto");
        return;
    }
    if (ctx->conf.verbose) {
        printf("Send %d bytes to %s:%d\n", n, inet_ntoa(ctx->acceptor_addr->sin_addr),
                            ntohs(ctx->acceptor_addr->sin_port));
    }

    ctx->cur_inst++;
}

int start_coordinator(Config *conf) {
    CoordinatorCtx *ctx = proposer_ctx_new(*conf);
    ctx->base = event_base_new();
    struct hostent *server;
    int serverlen;
    ctx->acceptor_addr = malloc( sizeof (struct sockaddr_in) );

    // socket to send Paxos messages to acceptors
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("cannot create socket");
        return EXIT_FAILURE;
    }
    ctx->sock = sock;

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

    struct event *ev_recv;
    int listen_socket = create_server_socket(conf->proposer_port);
    addMembership(conf->proposer_addr, listen_socket);
    ev_recv = event_new(ctx->base, listen_socket, EV_READ|EV_PERSIST, on_value, ctx);

    event_add(ev_recv, NULL);

    event_base_dispatch(ctx->base);
    // close(client_sock);
    close(sock);
    return EXIT_SUCCESS;
}

