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
#include <pthread.h>
#include "netpaxos_utils.h"
#include "config.h"
#include "application.h"
#include "message.h"

struct client_state {
    int mps;
    int sock;
    struct sockaddr_in *proposer;
    struct timespec send_time;
    struct event_base *base;
    int verbose;
    int vlen;
    char *payload;
    int payload_sz;
    int src_port;
    int running;
    struct mmsghdr *out_msgs;
    struct iovec *out_iovecs;
    struct mmsghdr *msgs;
    struct iovec *iovecs;
    char **bufs;
    struct timespec timeout;
    FILE *fp;
    pthread_t recv_th;
};

void send_message(struct client_state *state);

void signal_handler(evutil_socket_t fd, short what, void *arg) {
    struct client_state *state = (struct client_state*) arg;
    if (what&EV_SIGNAL) {
        state->running = 0;
        event_base_loopbreak(state->base);
        pthread_cancel(state->recv_th);
        printf("Stop client\n");
    }
}


void monitor(evutil_socket_t fd, short what, void *arg) {
    struct client_state *state = (struct client_state *) arg;
    if ( state->mps ) {
        fprintf(stdout, "%d\n", state->mps);
    }
    state->mps = 0;
}


void *thread_loop(void *arg) {
    struct client_state *state = arg;
    struct timespec recv_time, result;
    while(state->running) {
        int retval = recvmmsg(state->sock, state->msgs, state->vlen, 0, NULL);
        if (retval < 0) {
          perror("recvmmsg()");
          exit(EXIT_FAILURE);
        }
        state->mps += retval;

        int i;
        for (i = 0; i < retval; i++) {
            state->bufs[i][state->msgs[i].msg_len] = 0;
            if (state->verbose) {
                printf("Received %d messages\n", retval);
                printf("%d %s %d\n", i+1, state->bufs[i], state->msgs[i].msg_len);
            }
        }
        gettime(&recv_time);
        int negative = timediff(&result, &recv_time, &state->send_time);
        if (negative) {
            fprintf(stderr, "Latency is negative\n");
        } else {
            double latency = (result.tv_sec + ((double)result.tv_nsec) / 1e9);
            fprintf(state->fp, "%.9f\n", latency);
        }
        // clean receiving buffer
        send_message(state);
        gettime(&state->send_time);
    }
    return NULL;
}

void send_message(struct client_state *state) {
    if (state->running) {
        int r = sendmmsg(state->sock, state->out_msgs, state->vlen, 0);
        if (r <= 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
            }

            if (errno == ECONNREFUSED) {
            }
            perror("sendmmsg()");
        }
        if (state->verbose) {
            printf("Send %d messages\n", r);
        }
    }
}

struct client_state* client_state_new(Config *conf) {
    struct client_state *state = malloc(sizeof(struct client_state));
    state->base = event_base_new();
    state->running = 1;
    state->payload = malloc(32);
    char key[] = "abcde123456789";
    char value[] = "zxcvbnmasdfghj";
    state->payload[0] = PUT;
    char ksize = (unsigned char) strlen(key);
    char vsize = (unsigned char) strlen(value);
    state->payload[1] = ksize;
    state->payload[2] = vsize;
    memcpy(&state->payload[3], key, ksize);
    memcpy(&state->payload[ 3 + ksize ], value, vsize);

    state->payload_sz = ksize + vsize + 4; // 3 for three chars and 1 for terminator

    state->vlen = conf->vlen;
    state->mps = 0;
    state->msgs = calloc(state->vlen, sizeof(struct mmsghdr));
    state->iovecs = calloc(state->vlen, sizeof(struct iovec));
    state->out_msgs = calloc(state->vlen, sizeof(struct mmsghdr));
    state->out_iovecs = calloc(state->vlen, sizeof(struct iovec));
    state->bufs = calloc(state->vlen, sizeof(char*));
    int i;
    for (i = 0; i < state->vlen; i++) {
        state->bufs[i] = malloc(BUFSIZE + 1);
    }
    return state;
}

void client_state_free(struct client_state *state) {
    fclose(state->fp);
    event_base_free(state->base);
    free(state->payload);
    free(state->msgs);
    free(state->iovecs);
    free(state->out_msgs);
    free(state->out_iovecs);
    free(state->proposer);

    int i;
    for (i = 0; i < state->vlen; i++) {
        free(state->bufs[i]);
    }
    free(state->bufs);
    free(state);
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        printf("Usage: %s config output\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    Config *conf = parse_conf(argv[1]);
    struct client_state *state = client_state_new(conf);
    state->verbose = conf->verbose;
    struct sockaddr_in *proposer = malloc(sizeof (struct sockaddr_in));
    // socket to send Paxos messages to learners
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("cannot create socket");
        return EXIT_FAILURE;
    }

    state->sock = sock;

    struct hostent *server = gethostbyname(conf->proposer_addr);

    if (server == NULL) {
        fprintf(stderr, "ERROR, no such host as %s\n", conf->proposer_addr);
        return EXIT_FAILURE;
    }
    /* build the server's Internet address */
    bzero((char *) proposer, sizeof(struct sockaddr_in));
    proposer->sin_family = AF_INET;
    bcopy((char *)server->h_addr,
      (char *)&(proposer->sin_addr.s_addr), server->h_length);
    proposer->sin_port = htons(conf->proposer_port);

    state->proposer = proposer;
    state->timeout.tv_sec = TIMEOUT;
    state->timeout.tv_sec = 0;

   int i;
    for (i = 0; i < state->vlen; i++) {
        state->iovecs[i].iov_base         = (void*)state->bufs[i];
        state->iovecs[i].iov_len          = BUFSIZE;
        state->msgs[i].msg_hdr.msg_iov    = &state->iovecs[i];
        state->msgs[i].msg_hdr.msg_iovlen = 1;

        state->out_iovecs[i].iov_base         = (void*)state->payload;
        state->out_iovecs[i].iov_len          = state->payload_sz;
        state->out_msgs[i].msg_hdr.msg_name    = (void *)state->proposer;
        state->out_msgs[i].msg_hdr.msg_namelen = sizeof(struct sockaddr_in);
        state->out_msgs[i].msg_hdr.msg_iov    = &state->out_iovecs[i];
        state->out_msgs[i].msg_hdr.msg_iovlen = 1;
    }

    state->fp = fopen(argv[2], "w+");

    if(pthread_create(&state->recv_th, NULL, thread_loop, state)) {
        fprintf(stderr, "Error creating thread\n");
        exit(EXIT_FAILURE);
    }
    send_message(state);

    struct timeval period = {1, 0};
    struct event *ev_monitor;
    ev_monitor = event_new(state->base, -1, EV_TIMEOUT|EV_PERSIST, monitor, state);
    struct event *ev_sigint;
    ev_sigint = evsignal_new(state->base, SIGTERM, signal_handler, state);

    event_add(ev_monitor, &period);
    event_add(ev_sigint, NULL);

    event_base_dispatch(state->base);
    free(conf);
    event_free(ev_monitor);
    event_free(ev_sigint);
    client_state_free(state);
    close(sock);

    if(pthread_join(state->recv_th, NULL)) {
        fprintf(stderr, "Error joining thread\n");
        exit(EXIT_FAILURE);
    }
    return EXIT_SUCCESS;
}

