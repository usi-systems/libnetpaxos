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
#include "proposer.h"
#include "message.h"
#include "netpaxos_utils.h"
#include "config.h"

#define BUF_SIZE 32

ProposerCtx *proposer_ctx_new(Config conf);
void proposer_ctx_destroy(ProposerCtx *st);
void submit(void *arg);
void propose_value(ProposerCtx *ctx, void *arg);


static void
echo_read_cb(struct bufferevent *bev, void *arg)
{
    ProposerCtx *ctx = (ProposerCtx *) arg;
    /* This callback is invoked when there is data to read on bev. */
    struct evbuffer *input = bufferevent_get_input(bev);
    size_t len = evbuffer_get_length(input);
    if (len) {
        evbuffer_remove(input, ctx->buf, len);
        propose_value(ctx, ctx->buf);
    }
}

static void
echo_event_cb(struct bufferevent *bev, short events, void *ctx)
{
    if (events & BEV_EVENT_ERROR)
            perror("Error from bufferevent");
    if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
            bufferevent_free(bev);
    }
}

static void
accept_conn_cb(struct evconnlistener *listener,
    evutil_socket_t fd, struct sockaddr *address, int socklen,
    void *arg)
{
    ProposerCtx *ctx = (ProposerCtx *) arg;
    /* We got a new connection! Set up a bufferevent for it. */
    struct event_base *base = evconnlistener_get_base(listener);
    struct bufferevent *bev = bufferevent_socket_new(
            base, fd, BEV_OPT_CLOSE_ON_FREE);

    bufferevent_setcb(bev, echo_read_cb, NULL, echo_event_cb, ctx);

    bufferevent_enable(bev, EV_READ|EV_WRITE);
    // Assume there is only one client
    ctx->bev = bev;
}

static void
accept_error_cb(struct evconnlistener *listener, void *ctx)
{
        struct event_base *base = evconnlistener_get_base(listener);
        int err = EVUTIL_SOCKET_ERROR();
        fprintf(stderr, "Got an error %d (%s) on the listener. "
                "Shutting down.\n", err, evutil_socket_error_to_string(err));

        event_base_loopexit(base, NULL);
}


ProposerCtx *proposer_ctx_new(Config conf) {
    ProposerCtx *ctx = malloc(sizeof(ProposerCtx));
    ctx->conf = conf;
    ctx->mps = 0;
    ctx->avg_lat = 0.0;
    ctx->cur_inst = 0;
    ctx->acked_packets = 0;
    ctx->buf = malloc(BUF_SIZE);
    ctx->msg = malloc(sizeof(Message) + 1); bzero(ctx->msg, sizeof(Message) + 1);
    char fname[32];
    int n = snprintf(fname, sizeof fname, "proposer-%d.txt", conf.node_id);
    if ( n < 0 || n >= sizeof fname )
        exit(EXIT_FAILURE);
    ctx->fp = fopen(fname, "w+");
    return ctx;
}

void proposer_ctx_destroy(ProposerCtx *ctx) {
    free(ctx->buf);
    free(ctx->msg); free(ctx);
}


void proposer_signal_handler(evutil_socket_t fd, short what, void *arg) {
    ProposerCtx *ctx = (ProposerCtx *) arg;
    if (what&EV_SIGNAL) {
        event_base_loopbreak(ctx->base);
        fprintf(stdout, "sent_inst: %d\n", ctx->cur_inst);
        fprintf(stdout, "acked_packets: %d\n", ctx->acked_packets);
        proposer_ctx_destroy(ctx);
    }
}


void perf_cb(evutil_socket_t fd, short what, void *arg)
{
    ProposerCtx *ctx = (ProposerCtx *) arg;
    if ( ctx->mps ) {
        fprintf(stdout, "%d,%.6f\n", ctx->mps, (ctx->avg_lat / ctx->mps));
    }
    ctx->mps = 0;
    ctx->avg_lat = 0;
}


void recv_cb(evutil_socket_t fd, short what, void *arg)
{
    ProposerCtx *ctx = (ProposerCtx *) arg;
    if (what&EV_READ) {
        struct sockaddr_in remote;
        socklen_t remote_len = sizeof(remote);
        Message msg;
        int n = recvfrom(fd, &msg, sizeof(msg), 0, (struct sockaddr *) &remote, &remote_len);
        if (n < 0) {
          perror("ERROR in recvfrom");
          return;
        }
        unpack(ctx->msg, &msg);
        
        if (ctx->conf.verbose) print_message(ctx->msg);

        ctx->acked_packets++;
        ctx->mps++;
        bufferevent_write(ctx->bev, msg.paxosval, strlen(msg.paxosval));

        if (ctx->acked_packets >= ctx->conf.maxinst) {
            raise(SIGTERM);
        }
    }
}


void propose_value(ProposerCtx *ctx, void *arg)
{
    char *v = (char*) arg;
    socklen_t serverlen = sizeof(*ctx->serveraddr);
    Message msg;
    initialize_message(&msg, ctx->conf.paxos_msgtype);
    if (ctx->cur_inst >= ctx->conf.maxinst) {
        return;
    }
    strncpy(msg.paxosval, v, PAXOS_VALUE_SIZE-1);
    if (ctx->conf.verbose) print_message(&msg);
    

    pack(ctx->msg, &msg);
    if (sendto(ctx->learner_sock, ctx->msg, sizeof(Message), 0, 
            (struct sockaddr*) ctx->serveraddr, serverlen) < 0) {
        perror("ERROR in sendto");
        return;
    }

    ctx->cur_inst++;

}


int start_proposer(Config *conf, void *(*result_cb)(void* arg)) {
    ProposerCtx *ctx = proposer_ctx_new(*conf);
    ctx->base = event_base_new();
    ctx->result_cb = result_cb;
    event_base_priority_init(ctx->base, 4);
    struct hostent *server;
    int serverlen;
    ctx->serveraddr = malloc( sizeof (struct sockaddr_in) );

    int learner_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (learner_sock < 0) {
        perror("cannot create socket");
        return EXIT_FAILURE;
    }
    ctx->learner_sock = learner_sock;

    server = gethostbyname(conf->learner_addr);
    if (server == NULL) {
        fprintf(stderr, "ERROR, no such host as %s\n", conf->learner_addr);
        return EXIT_FAILURE;
    }

    /* build the server's Internet address */
    bzero((char *) ctx->serveraddr, sizeof(struct sockaddr_in));
    ctx->serveraddr->sin_family = AF_INET;
    bcopy((char *)server->h_addr,
      (char *)&(ctx->serveraddr->sin_addr.s_addr), server->h_length);
    ctx->serveraddr->sin_port = htons(conf->learner_port);

    struct event *ev_recv, *ev_perf, *evsig;
    struct timeval perf_tm = {1, 0};
    ev_recv = event_new(ctx->base, learner_sock, EV_READ|EV_PERSIST, recv_cb, ctx);
    ev_perf = event_new(ctx->base, -1, EV_TIMEOUT|EV_PERSIST, perf_cb, ctx);
    evsig = evsignal_new(ctx->base, SIGTERM, proposer_signal_handler, ctx);

    event_priority_set(evsig, 0);
    event_priority_set(ev_perf, 1);
    event_priority_set(ev_recv, 3);

    event_add(ev_recv, NULL);
    event_add(ev_perf, &perf_tm);
    event_add(evsig, NULL);

    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    /* This is an INET address */
    sin.sin_family = AF_INET;
    /* Listen on 0.0.0.0 */
    sin.sin_addr.s_addr = htonl(0);
    /* Listen on the given port. */
    sin.sin_port = htons(conf->proposer_port);
    struct evconnlistener *listener;
    listener = evconnlistener_new_bind(ctx->base, accept_conn_cb, ctx,
        LEV_OPT_LEAVE_SOCKETS_BLOCKING|LEV_OPT_CLOSE_ON_FREE|LEV_OPT_REUSEABLE, -1,
    (struct sockaddr*)&sin, sizeof(sin));
    if (!listener) {
        perror("Couldn't create listener");
        return 1;
    }
    evconnlistener_set_error_cb(listener, accept_error_cb);

    event_base_dispatch(ctx->base);
    // close(client_sock);
    close(learner_sock);
    return EXIT_SUCCESS;
}

