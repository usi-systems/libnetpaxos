#include <stdio.h>
#include <event2/event.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <event2/event.h>
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
// static char *rand_string(char *str, size_t size)
// {
//     const char charset[] = "QWERTYUIOPASDFGHJKLZXCVBNM!@#$^&*()1234567890";
//     if (size) {
//         --size;
//         size_t n;
//         for (n = 0; n < size; n++) {
//             int key = rand() % (int) (sizeof charset - 1);
//             str[n] = charset[key];
//         }
//         str[size] = '\0';
//     }
//     return str;
// }

ProposerCtx *proposer_ctx_new(Config conf) {
    ProposerCtx *ctx = malloc(sizeof(ProposerCtx));
    ctx->conf = conf;
    ctx->mps = 0;
    ctx->avg_lat = 0.0;
    ctx->cur_inst = 0;
    ctx->acked_packets = 0;
    ctx->buf = malloc(BUF_SIZE);
    ctx->msg = malloc(sizeof(Message) + 1);
    bzero(ctx->msg, sizeof(Message) + 1);
    char fname[32];
    int n = snprintf(fname, sizeof fname, "proposer-%d.txt", conf.node_id);
    if ( n < 0 || n >= sizeof fname )
        exit(EXIT_FAILURE);
    ctx->fp = fopen(fname, "w+");
    return ctx;
}

void proposer_ctx_destroy(ProposerCtx *ctx) {
    fclose(ctx->fp);
    free(ctx->buf);
    free(ctx->msg);
    free(ctx);
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
        if (ctx->acked_packets >= ctx->conf.maxinst) {
            raise(SIGTERM);
        }
    }
}


void receive_request(evutil_socket_t fd, short what, void *arg)
{
    ProposerCtx *ctx = (ProposerCtx *) arg;
    if (what&EV_READ) {
        struct sockaddr_in remote;
        socklen_t remote_len = sizeof(remote);
        int n = recvfrom(fd, ctx->buf, BUF_SIZE, 0, (struct sockaddr *) &remote, &remote_len);
        if (n < 0) {
          perror("ERROR in recvfrom");
          return;
        }
        propose_value(ctx, ctx->buf);
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
    if (ctx->conf.verbose) print_message(&msg);
    
    strncpy(msg.paxosval, v, PAXOS_VALUE_SIZE-1);

    pack(ctx->msg, &msg);
    if (sendto(ctx->learner_sock, ctx->msg, sizeof(Message), 0, 
            (struct sockaddr*) ctx->serveraddr, serverlen) < 0) {
        perror("ERROR in sendto");
        return;
    }

    ctx->cur_inst++;

}

int create_server_socket(Config *conf) {
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        perror("cannot create socket");
        return EXIT_FAILURE;
    }
    struct sockaddr_in serv_addr;
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(conf->proposer_port);
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        perror("ERROR on binding");
        return EXIT_FAILURE;
    }
    return fd;
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

    server = gethostbyname(conf->server);
    if (server == NULL) {
        fprintf(stderr, "ERROR, no such host as %s\n", conf->server);
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

    // struct timeval send_interval = { conf->second, conf->microsecond };
    // struct timeval tv_in = { conf->second, conf->microsecond };
    // const struct timeval *tv_out;
    // tv_out = event_base_init_common_timeout(ctx->base, &tv_in);
    // memcpy(&send_interval, tv_out, sizeof(struct timeval));
    // int i = 0;
    // for (i; i < conf->outstanding; i++) {
    //     struct event *ev_send;
    //     ev_send = event_new(ctx->base, learner_sock, EV_TIMEOUT|EV_PERSIST, send_value, ctx);
    //     event_priority_set(ev_send, 2);
    //     event_add(ev_send, &send_interval);
    // }

    int client_sock = create_server_socket(conf);
    ctx->client_sock = client_sock;
    struct event *ev_receive_request;
    ev_receive_request = event_new(ctx->base, client_sock, EV_READ|EV_PERSIST, receive_request, ctx);
    event_add(ev_receive_request, NULL);

    event_base_dispatch(ctx->base);
    close(client_sock);
    close(learner_sock);
    return EXIT_SUCCESS;
}

