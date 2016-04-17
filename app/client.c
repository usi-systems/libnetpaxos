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

struct address {
    struct sockaddr_in *addr;
    int port;
};

void on_response(evutil_socket_t fd, short what, void *arg) {

}

void on_timeout(evutil_socket_t fd, short what, void *arg) {
    struct address *addr = (struct address*) arg;
    socklen_t len = sizeof(struct sockaddr_in);
    char msg[] = "Put (key, value)";
    int n = sendto(fd, msg, sizeof(msg), 0, (struct sockaddr*) addr->addr, len);
    if (n < 0) {
        perror("ERROR in sendto");
        return;
    }

}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        printf("Usage: %s address port\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    struct event_base *base = event_base_new();
    struct sockaddr_in *learner_addr = malloc(sizeof (struct sockaddr_in));
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
    bzero((char *) learner_addr, sizeof(struct sockaddr_in));
    learner_addr->sin_family = AF_INET;
    bcopy((char *)server->h_addr,
      (char *)&(learner_addr->sin_addr.s_addr), server->h_length);
    learner_addr->sin_port = htons(port);

    struct address *addr = malloc(sizeof(struct address));
    addr->addr = learner_addr;
    addr->port = port;

    struct event *ev_recv;
    ev_recv = event_new(base, sock, EV_READ|EV_PERSIST, on_response, addr);

    struct event *ev_send;
    ev_send = event_new(base, sock, EV_TIMEOUT|EV_PERSIST, on_timeout, addr);
    struct timeval period = {1, 0};

    event_add(ev_recv, NULL);
    event_add(ev_send, &period);

    event_base_dispatch(base);
    close(sock);
    return EXIT_SUCCESS;
}

