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
#include <stdlib.h>
#include "message.h"
#include "learner.h"
#include "netpaxos_utils.h"
#include "config.h"
/* Here's a callback function that calls loop break */
LearnerCtx *learner_ctx_new(Config conf) {
    LearnerCtx *ctx = malloc(sizeof(LearnerCtx));
    ctx->conf = conf;
    ctx->mps = 0;
    ctx->num_packets = 0;
    ctx->values = malloc(conf.maxinst * sizeof(int));
    if (ctx->values == NULL) {
        perror("Unable to allocate memory for values\n");
    }
    ctx->buffer = malloc(conf.bufsize);
    if (ctx->buffer == NULL) {
        perror("Unable to allocate memory for buffer\n");
    }
    return ctx;
}

void learner_ctx_destroy(LearnerCtx *ctx) {
    free(ctx->buffer);
    free(ctx->values);
    free(ctx);
}

void signal_handler(evutil_socket_t fd, short what, void *arg) {
    LearnerCtx *ctx = (LearnerCtx *) arg;
    if (what&EV_SIGNAL) {
        event_base_loopbreak(ctx->base);
        FILE *fp = fopen("learner.txt", "w+");
        int i;
        for (i = 0; i <= ctx->conf.maxinst; i++) {
            fprintf(fp, "%d\n", ctx->values[i]);
        }
        fprintf(fp, "num_packets: %d\n", ctx->num_packets);
        fclose(fp);
        learner_ctx_destroy(ctx);
    }
}

void monitor(evutil_socket_t fd, short what, void *arg) {
    LearnerCtx *ctx = (LearnerCtx *) arg;
    if ( ctx->mps ) {
        printf("%d\n", ctx->mps);
    }
    ctx->mps = 0;
}

void cb_func(evutil_socket_t fd, short what, void *arg)
{
    LearnerCtx *ctx = (LearnerCtx *) arg;
    struct timespec end, result;
    struct sockaddr_in remote;
    socklen_t remote_len = sizeof(remote);
    double latency;
    size_t msglen;
    int n;
    if (ctx->conf.enable_paxos) {
        Message msg;
        n = recvfrom(fd, &msg, sizeof(msg), 0, (struct sockaddr *) &remote, &remote_len);
        if (n < 0) {
          perror("ERROR in recvfrom");
          return;
        }
        unpack(&msg);
        if (ctx->conf.verbose) {
            message_to_string(msg, ctx->buffer);
            printf("%s" , ctx->buffer);
        }
        if (msg.inst >= ctx->conf.maxinst) {
            return;
        }
        ctx->values[msg.inst] = msg.value;
        pack(&msg, ctx->buffer);
        msglen = sizeof(Message);
        n = sendto(fd, ctx->buffer, msglen, 0, (struct sockaddr*) &remote, remote_len);
        if (n < 0) {
            perror("ERROR in sendto");
            return;
        }

    } else {
        TimespecMessage msg;
        n = recvfrom(fd, &msg, sizeof(msg), 0, (struct sockaddr *) &remote, &remote_len);
        if (n < 0) {
            perror("ERROR in recvfrom");
            return;
        }
        n = sendto(fd, &msg, sizeof(msg), 0, (struct sockaddr*) &remote, remote_len);
        if (n < 0) {
            perror("ERROR in sendto");
            return;
        }
    }

    ctx->mps++;
    ctx->num_packets++;

    if (ctx->num_packets == ctx->conf.maxinst) {
        raise(SIGTERM);
    }
}

int start_learner(Config *conf) {
    LearnerCtx *ctx = learner_ctx_new(*conf);
    ctx->base = event_base_new();
    event_base_priority_init(ctx->base, 4);
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        perror("cannot create socket");
        return EXIT_FAILURE;
    }

    struct sockaddr_in serv_addr;
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(conf->learner_port);
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(fd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        perror("ERROR on binding");
        return EXIT_FAILURE;
    }
    struct event *recv_ev, *monitor_ev, *evsig;
    recv_ev = event_new(ctx->base, fd, EV_READ|EV_PERSIST, cb_func, ctx);
    struct timeval timeout = {1, 0};
    monitor_ev = event_new(ctx->base, -1, EV_TIMEOUT|EV_PERSIST, monitor, ctx);
    evsig = evsignal_new(ctx->base, SIGTERM, signal_handler, ctx);
    event_priority_set(evsig, 0);
    event_priority_set(monitor_ev, 1);
    event_priority_set(recv_ev, 2);
    
    event_add(recv_ev, NULL);
    event_add(monitor_ev, &timeout);
    /* Signal event to terminate event loop */
    event_add(evsig, NULL);

    event_base_dispatch(ctx->base);
    return EXIT_SUCCESS;
}