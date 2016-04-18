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

#define BUF_SIZE 1500
void send_message(evutil_socket_t fd, struct sockaddr_in *addr);


struct client_state {
    int mps;
    struct sockaddr_in *proposer;
};

void monitor(evutil_socket_t fd, short what, void *arg) {
    struct client_state *state = (struct client_state *) arg;
    if ( state->mps ) {
        fprintf(stdout, "%d\n", state->mps);
    }
    state->mps = 0;
}


void on_response(evutil_socket_t fd, short what, void *arg) {
    struct client_state *state = (struct client_state*) arg;
    if (what&EV_READ) {
        struct sockaddr_in remote;
        socklen_t remote_len = sizeof(remote);
        char recvbuf[BUF_SIZE];
        int n = recvfrom(fd, recvbuf, BUF_SIZE, 0, (struct sockaddr *) &remote, &remote_len);
        if (n < 0) {
          perror("ERROR in recvfrom");
          return;
        }
        // printf("on value: %s: %d length, addr_length: %d\n", recvbuf, n, remote_len);
        send_message(fd, state->proposer);
        state->mps++;
    } else if (what&EV_TIMEOUT) {
        // printf("on timeout, send.\n");
        send_message(fd, state->proposer);
    }
}

void send_message(evutil_socket_t fd, struct sockaddr_in *addr) {
    socklen_t len = sizeof(struct sockaddr_in);
    // Warning: Fit message in 16 Bytes
    char msg[] = "Put (key, val)";
    int n = sendto(fd, msg, sizeof(msg), 0, (struct sockaddr*) addr, len);
    if (n < 0) {
        perror("ERROR in sendto");
        return;
    }
}

struct client_state* client_state_new() {
    struct client_state *state = malloc(sizeof(struct client_state*));
    state->mps = 0;
    return state;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        printf("Usage: %s address port\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    struct client_state *state = client_state_new();
    struct event_base *base = event_base_new();
    struct sockaddr_in *proposer = malloc(sizeof (struct sockaddr_in));
    // socket to send Paxos messages to learners
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("cannot create socket");
        return EXIT_FAILURE;
    }

    struct hostent *server = gethostbyname(argv[1]);
    int port = atoi(argv[2]);

    if (server == NULL) {
        fprintf(stderr, "ERROR, no such host as %s\n", argv[1]);
        return EXIT_FAILURE;
    }
    /* build the server's Internet address */
    bzero((char *) proposer, sizeof(struct sockaddr_in));
    proposer->sin_family = AF_INET;
    bcopy((char *)server->h_addr,
      (char *)&(proposer->sin_addr.s_addr), server->h_length);
    proposer->sin_port = htons(port);

    state->proposer = proposer;

    struct event *ev_recv;
    struct timeval period = {1, 0};
    ev_recv = event_new(base, sock, EV_READ|EV_TIMEOUT|EV_PERSIST, on_response, state);
    struct event *ev_monitor;
    ev_monitor = event_new(base, -1, EV_TIMEOUT|EV_PERSIST, monitor, state);


    event_add(ev_recv, &period);
    event_add(ev_monitor, &period);

    event_base_dispatch(base);
    close(sock);
    return EXIT_SUCCESS;
}

