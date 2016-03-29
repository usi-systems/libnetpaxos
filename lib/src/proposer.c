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

ProposerCtx *proposer_ctx_new(Config conf) {
    ProposerCtx *ctx = malloc(sizeof(ProposerCtx));
    ctx->conf = conf;
    ctx->mps = 0;
    ctx->avg_lat = 0.0;
    ctx->cur_inst = 0;
    ctx->acked_packets = 0;
    ctx->values = malloc(conf.maxinst * sizeof(int));
    ctx->buffer = malloc(sizeof(Message));
    char fname[32];
    int n = snprintf(fname, sizeof fname, "proposer%d.txt", getpid());
    if ( n < 0 || n >= sizeof fname )
        exit(EXIT_FAILURE);
    ctx->fp = fopen(fname, "w+");
    return ctx;
}

void proposer_ctx_destroy(ProposerCtx *ctx) {
    fclose(ctx->fp);
    free(ctx->buffer);
    free(ctx->values);
    free(ctx);
}


void proposer_signal_handler(evutil_socket_t fd, short what, void *arg) {
    ProposerCtx *ctx = (ProposerCtx *) arg;
    if (what&EV_SIGNAL) {
        event_base_loopbreak(ctx->base);
        // int i;
        // for (i = 0; i < ctx->max_inst; i++) {
        //     fprintf(ctx->fp, "%d\n", ctx->values[i]);
        // }
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
        double latency;
        struct sockaddr_in remote;
        socklen_t remote_len = sizeof(remote);
        uint64_t coord_cycles = 0, acpt_cycles = 0, fwd_cycles = 0;
        struct timespec start, end, result;
        clock_gettime(CLOCK_MONOTONIC, &end);

        if (ctx->conf.enable_paxos) {
            Message msg;
            int n = recvfrom(fd, &msg, sizeof(msg), 0, (struct sockaddr *) &remote, &remote_len);
            if (n < 0) {
              perror("ERROR in recvfrom");
              return;
            }
            unpack(&msg);
            uint32_t coord_lat_high = (msg.ceh - msg.csh);
            uint32_t coord_lat_low = msg.cel - msg.csl;
            coord_cycles = ((coord_cycles + coord_lat_high) << 32) + coord_lat_low;
            if (ctx->conf.verbose) {
                message_to_string(msg, ctx->buffer);
                fprintf(stdout, "%s" , ctx->buffer);
            }
            // fprintf(stdout, "%d %d %ld\n", coord_lat_high, coord_lat_low, coord_cycles);

            uint32_t acpt_lat_high = (msg.aeh - msg.ash);
            uint32_t acpt_lat_low = msg.ael - msg.asl;
            acpt_cycles = ((acpt_cycles + acpt_lat_high) << 32) + acpt_lat_low;

            uint32_t fwd_lat_high = (msg.feh - msg.fsh);
            uint32_t fwd_lat_low = msg.fel - msg.fsl;
            fwd_cycles = ((fwd_cycles + fwd_lat_high) << 32) + fwd_lat_low;

            start = msg.ts;
        } else {
            TimespecMessage msg;
            int n = recvfrom(fd, &msg, sizeof(msg), 0, (struct sockaddr *) &remote, &remote_len);
            if (n < 0) {
              perror("ERROR in recvfrom");
              return;
            }
            start = msg.ts;
        }
        if (timediff(&result, &end, &start) == 1) {
            fprintf(stderr, "Latency is negative\n");
        }
        latency = (result.tv_sec + ((double)result.tv_nsec) / 1e9);
        fprintf(ctx->fp, "%.9f,%ld,%ld,%ld\n", latency, fwd_cycles, coord_cycles, acpt_cycles);
        ctx->avg_lat += latency;
        ctx->acked_packets++;
        ctx->mps++;

        if (ctx->acked_packets >= ctx->conf.maxinst) {
            raise(SIGTERM);
        }
    }
}


void send_value(evutil_socket_t fd, short what, void *arg)
{
    ProposerCtx *ctx = (ProposerCtx *) arg;
    socklen_t serverlen = sizeof(*ctx->serveraddr);
    size_t msglen;
    int n;
    if (ctx->conf.enable_paxos) {
        Message msg;
        msglen = sizeof(Message);
        msg.mstype = 0;
        if (ctx->conf.reset_paxos) {
            msg.mstype = 255;
        }
        msg.inst = 0;
        msg.rnd = 1;
        msg.vrnd = 0;
        msg.acpid = 0;
        msg.fsh = 0;
        msg.fsl = 0;
        msg.feh = 0;
        msg.fel = 0;
        msg.csh = 0;
        msg.csl = 0;
        msg.ceh = 0;
        msg.cel = 0;
        msg.ash = 0;
        msg.asl = 0;
        msg.aeh = 0;
        msg.ael = 0;
        msg.value = ctx->cur_inst;
        if (ctx->cur_inst >= ctx->conf.maxinst) {
            return;
        }
        ctx->values[ctx->cur_inst] = msg.value;
        clock_gettime(CLOCK_MONOTONIC, &msg.ts);
        pack(&msg, ctx->buffer);
        n = sendto(fd, ctx->buffer, msglen, 0, (struct sockaddr*) ctx->serveraddr, serverlen);
        if (n < 0) {
            perror("ERROR in sendto");
            return;
        }
    } else {
        TimespecMessage msg;
        msglen = sizeof(TimespecMessage);
        clock_gettime(CLOCK_MONOTONIC, &(msg.ts));
        n = sendto(fd, &msg, msglen, 0, (struct sockaddr*) ctx->serveraddr, serverlen);
        if (n < 0) {
            perror("ERROR in sendto");
            return;
        }
    }

    ctx->cur_inst++;
}

int start_proposer(Config *conf) {
    ProposerCtx *ctx = proposer_ctx_new(*conf);
    ctx->base = event_base_new();
    event_base_priority_init(ctx->base, 4);
    struct hostent *server;
    int serverlen;
    ctx->serveraddr = malloc( sizeof (struct sockaddr_in) );

    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        perror("cannot create socket");
        return EXIT_FAILURE;
    }

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
    ev_recv = event_new(ctx->base, fd, EV_READ|EV_PERSIST, recv_cb, ctx);
    ev_perf = event_new(ctx->base, -1, EV_TIMEOUT|EV_PERSIST, perf_cb, ctx);
    evsig = evsignal_new(ctx->base, SIGTERM, proposer_signal_handler, ctx);

    event_priority_set(evsig, 0);
    event_priority_set(ev_perf, 1);
    event_priority_set(ev_recv, 3);

    event_add(ev_recv, NULL);
    event_add(ev_perf, &perf_tm);
    event_add(evsig, NULL);

    struct timeval send_interval = { conf->second, conf->microsecond };
    struct timeval tv_in = { conf->second, conf->microsecond };
    const struct timeval *tv_out;
    tv_out = event_base_init_common_timeout(ctx->base, &tv_in);
    memcpy(&send_interval, tv_out, sizeof(struct timeval));
    int i = 0;
    for (i; i < conf->outstanding; i++) {
        struct event *ev_send;
        ev_send = event_new(ctx->base, fd, EV_TIMEOUT|EV_PERSIST, send_value, ctx);
        event_priority_set(ev_send, 2);
        event_add(ev_send, &send_interval);
    }

    event_base_dispatch(ctx->base);
    close(fd);
    return EXIT_SUCCESS;
}