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
#include "netpaxos_utils.h"
#include "config.h"

#include "application.h"

#define BUF_SIZE 1500
#define MSGSIZE 32

void send_message(evutil_socket_t fd, struct sockaddr_in *addr, int idx);


struct client_state {
    int mps;
    struct sockaddr_in *proposer;
    struct timespec send_time;
    struct event_base *base;
    int verbose;
    FILE *fp;
};

void signal_handler(evutil_socket_t fd, short what, void *arg) {
    struct client_state *state = (struct client_state*) arg;
    if (what&EV_SIGNAL) {
        event_base_loopbreak(state->base);
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


void on_response(evutil_socket_t fd, short what, void *arg) {
    struct client_state *state = (struct client_state*) arg;
    struct timespec recv_time, result;
    if (what&EV_READ) {
        state->mps++;
        struct sockaddr_in remote;
        socklen_t remote_len = sizeof(remote);
        char recvbuf[BUF_SIZE];
        int n = recvfrom(fd, recvbuf, BUF_SIZE, 0, (struct sockaddr *) &remote, &remote_len);
        if (n < 0) {
          perror("ERROR in recvfrom");
          return;
        }
        gettime(&recv_time);
        int negative = timediff(&result, &recv_time, &state->send_time);
        if (negative) {
            fprintf(stderr, "Latency is negative\n");
        } else {
            double latency = (result.tv_sec + ((double)result.tv_nsec) / 1e9);
            fprintf(state->fp, "%.9f\n", latency);
        }
        if (state->verbose) {
            printf("on value: %s: %d length, addr_length: %d\n", recvbuf, n, remote_len);
        }
        // clean receiving buffer
        memset(recvbuf, 0, BUF_SIZE);
        send_message(fd, state->proposer, state->mps);
        gettime(&state->send_time);
    } else if (what&EV_TIMEOUT) {
        // printf("on timeout, send.\n");
        send_message(fd, state->proposer, state->mps);
        gettime(&state->send_time);
    }
}

void send_message(evutil_socket_t fd, struct sockaddr_in *addr, int count) {
    char msg[MSGSIZE];
    int n, size;
    if (count % 2 == 0) {
        char command = PUT;
        char key[] = "key";
        char value[] = "val";
        msg[0] = command;
        msg[1] = (unsigned char) strlen(key);
        msg[2] = (unsigned char) strlen(value);
        memcpy(&msg[3], key, msg[1]);
        memcpy(&msg[3+msg[1]], value, msg[2]);
        size = msg[1] + msg[2] + 4; // 3 for three chars and 1 for terminator
    } else {
        char command = GET;
        char key[] = "key";
        msg[0] = command;
        msg[1] = (unsigned char) strlen(key);
        msg[2] = 1;
        memcpy(&msg[3], key, msg[1]);
        size = msg[1] + 4; // 3 for three chars and 1 for terminator
    }
    msg[size-1] = '\0';
    socklen_t len = sizeof(struct sockaddr_in);
    n = sendto(fd, msg, size, 0, (struct sockaddr*) addr, len);
    if (n < 0) {
        perror("ERROR in sendto");
        return;
    }
    bzero(msg, MSGSIZE);
}

struct client_state* client_state_new() {
    struct client_state *state = malloc(sizeof(struct client_state));
    state->base = event_base_new();
    state->mps = 0;
    return state;
}

void client_state_free(struct client_state *state) {
    fclose(state->fp);
    event_base_free(state->base);
    free(state->proposer);
    free(state);
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        printf("Usage: %s config output\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    Config *conf = parse_conf(argv[1]);
    struct client_state *state = client_state_new();
    state->verbose = conf->verbose;
    struct sockaddr_in *proposer = malloc(sizeof (struct sockaddr_in));
    // socket to send Paxos messages to learners
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("cannot create socket");
        return EXIT_FAILURE;
    }

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
    state->fp = fopen(argv[2], "w+");

    struct event *ev_recv;
    struct timeval period = {1, 0};
    ev_recv = event_new(state->base, sock, EV_READ|EV_TIMEOUT|EV_PERSIST, on_response, state);
    struct event *ev_monitor;
    ev_monitor = event_new(state->base, -1, EV_TIMEOUT|EV_PERSIST, monitor, state);
    struct event *ev_sigint;
    ev_sigint = evsignal_new(state->base, SIGTERM, signal_handler, state);

    event_add(ev_recv, &period);
    event_add(ev_monitor, &period);
    event_add(ev_sigint, NULL);

    event_base_dispatch(state->base);
    free(conf);
    event_free(ev_recv);
    event_free(ev_monitor);
    event_free(ev_sigint);
    client_state_free(state);
    close(sock);
    return EXIT_SUCCESS;
}
