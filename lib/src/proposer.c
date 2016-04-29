#define _GNU_SOURCE
#include <stdio.h>
#include <event2/event.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <sys/ioctl.h>
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
#include "message.h"
#include "proposer.h"

void signal_handler(evutil_socket_t fd, short what, void *arg) {
    struct proposer_state *state = (struct proposer_state*) arg;
    if (what&EV_SIGNAL) {
        event_base_loopbreak(state->base);
        printf("Stop proposer\n");
    }
}


void on_response(evutil_socket_t fd, short what, void *arg) {
    struct proposer_state *state = arg;
    if (what&EV_READ) {
        struct sockaddr_in remote;
        socklen_t remote_len = sizeof(remote);
        char recvbuf[BUFSIZE];
        bzero(recvbuf, BUFSIZE);
        int n = recvfrom(fd, recvbuf, BUFSIZE, 0, (struct sockaddr *) &remote, &remote_len);
        if (n < 0) {
          perror("ERROR in recvfrom");
          return;
        }
        recvbuf[n] = '\0';

        if (state->conf.verbose) {
            printf("on value: %s: %d length, addr_length: %d\n", recvbuf, n, remote_len);
        }
        state->deliver(recvbuf, n, state->app_ctx);
    }
}

void submit(char* msg, int msg_size, struct proposer_state *state, deliver_fn res_cb, void *arg) {
    state->app_ctx = arg;
    state->deliver = res_cb;

    Message m;
    initialize_message(&m, phase2a);
    memcpy(m.paxosval, msg, msg_size);
    m.paxosval[msg_size] = '\0';
    m.client = *state->mine;
    pack(&m);
    socklen_t serverlen = sizeof(*state->coordinator);
    int n = sendto(state->sock, &m, sizeof(Message), 0, (struct sockaddr*) state->coordinator, serverlen);
    if (n < 0) {
        perror("ERROR in sendto");
        return;
    }
    struct event *ev_recv;
    ev_recv = event_new(state->base, state->sock, EV_READ|EV_PERSIST, on_response, state);
    event_add(ev_recv, NULL);
    event_base_dispatch(state->base);
}

struct proposer_state* proposer_state_new(Config *conf) {
    struct proposer_state *state = malloc(sizeof(struct proposer_state));
    state->conf = *conf;
    state->base = event_base_new();
    return state;
}

int init_proposer(struct proposer_state *state, char* interface) {
    struct sockaddr_in *coordinator = malloc(sizeof (struct sockaddr_in));
    state->mine = malloc(sizeof (struct sockaddr_in));
    socklen_t len = sizeof(struct sockaddr_in);
    int sock = create_server_socket(0);
    if (sock < 0) {
        perror("cannot create socket");
        return EXIT_FAILURE;
    }
    state->sock = sock;
    struct hostent *server = gethostbyname(state->conf.coordinator_addr);
    if (server == NULL) {
        fprintf(stderr, "ERROR, no such host as %s\n", state->conf.coordinator_addr);
        return EXIT_FAILURE;
    }
    if (getsockname(sock, (struct sockaddr *)state->mine, &len) == -1) {
        perror("getsockname");
        return EXIT_FAILURE;
    }
    struct ifreq ifr;
    ifr.ifr_addr.sa_family = AF_INET;
    strncpy(ifr.ifr_name, interface, IFNAMSIZ-1);
    ioctl(sock, SIOCGIFADDR, &ifr);
    
    struct sockaddr_in *tmp = (struct sockaddr_in *)&ifr.ifr_addr;
    bcopy(&tmp->sin_addr, &state->mine->sin_addr, sizeof(struct in_addr));

    // printf("address %s, port %d\n", inet_ntoa(state->mine->sin_addr), ntohs(state->mine->sin_port));

    /* build the server's Internet address */
    bzero((char *) coordinator, sizeof(struct sockaddr_in));
    coordinator->sin_family = AF_INET;
    bcopy((char *)server->h_addr,
      (char *)&(coordinator->sin_addr.s_addr), server->h_length);
    coordinator->sin_port = htons(state->conf.coordinator_port);
    state->coordinator = coordinator;
    return 0;
}


void free_proposer(struct proposer_state *state) {
    event_base_free(state->base);
    free(state->coordinator);
    free(state);
}

struct proposer_state *make_proposer(char *config_file, char* interface) {
    Config *conf = parse_conf(config_file);
    struct proposer_state *state = proposer_state_new(conf);
    int res  = init_proposer(state, interface);
    if (res < 0) {
        free_proposer(state);
        perror("init_proposer");
        exit(EXIT_FAILURE);
    }
    free(conf);
    struct event *ev_sigterm;
    ev_sigterm = evsignal_new(state->base, SIGTERM, signal_handler, state);
    struct event *ev_sigint;
    ev_sigint = evsignal_new(state->base, SIGINT, signal_handler, state);
    event_add(ev_sigint, NULL);
    event_add(ev_sigterm, NULL);

    return state;
}

