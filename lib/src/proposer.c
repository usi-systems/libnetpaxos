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
#include <netinet/udp.h>   //Provides declarations for udp header
#include <netinet/ip.h>    //Provides declarations for ip header

int retry (struct proposer_state *ctx);

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
        struct timespec result, end;
        gettime(&end);
        int negative = timediff(&result, &end, &state->start);
        if (negative) {
            fprintf(stderr, "Latency is negative\n");
        } else {
            double latency = (result.tv_sec + ((double)result.tv_nsec) / 1e9);
            fprintf(stdout, "%.9f\n", latency);
        }
        if (state->conf->verbose) {
            printf("on value: %s: %d length, addr_length: %d\n", recvbuf, n, remote_len);
        }
        state->deliver(recvbuf, n, state->app_ctx);
    } else if (what&EV_TIMEOUT) {
        retry(state);
    }
}

void init_rawsock (struct proposer_state *ctx, struct sockaddr_in *mine, struct sockaddr_in *dest)
{
    //zero out the packet buffer
    memset (ctx->datagram, 0, BUFSIZE);
    //IP header
    struct iphdr *iph = (struct iphdr *) ctx->datagram;
    //UDP header
    struct udphdr *udph = (struct udphdr *) (ctx->datagram + sizeof (struct ip));
    //Fill in the IP Header
    iph->ihl = 5;
    iph->version = 4;
    iph->tos = 0;
    iph->id = htonl (54321); //Id of this packet
    iph->frag_off = 0;
    iph->ttl = 255;
    iph->protocol = IPPROTO_UDP;
    iph->check = 0;      //Set to 0 before calculating checksum
    iph->saddr = mine->sin_addr.s_addr;    //Spoof the source ip address
    iph->daddr = dest->sin_addr.s_addr;
    //UDP header
    udph->source = mine->sin_port;
    udph->dest = dest->sin_port;
    udph->check = 0; //leave checksum 0 now, filled later by pseudo header
}

int retry (struct proposer_state *ctx) {
    struct iphdr *iph = (struct iphdr *) ctx->datagram;
    if (sendto (ctx->rawsock, ctx->datagram, iph->tot_len,  0, (struct sockaddr *) ctx->dest, sizeof (*ctx->dest)) < 0) {
        perror("sendto failed");
    }
    gettime(&ctx->start);
    return 0;
}

int paxos_send (struct proposer_state *ctx, char *msg, int msglen) {
    char *data;
    //Data part
    data = ctx->datagram + sizeof(struct iphdr) + sizeof(struct udphdr);
    memcpy(data , msg, msglen);
    //Ip checksum
    struct iphdr *iph = (struct iphdr *) ctx->datagram;
    iph->check = csum ((unsigned short *) ctx->datagram, iph->tot_len);
    iph->tot_len = sizeof (struct iphdr) + sizeof (struct udphdr) + msglen;
    //UDP header
    struct udphdr *udph = (struct udphdr *) (ctx->datagram + sizeof (struct ip));
    udph->len = htons(8 + msglen); //udp header size
    udph->check = 0; //leave checksum 0 now, filled later by pseudo header

    if (sendto (ctx->rawsock, ctx->datagram, iph->tot_len,  0, (struct sockaddr *) ctx->dest, sizeof (*ctx->dest)) < 0) {
        perror("sendto failed");
    }
    gettime(&ctx->start);
    return 0;
}

void submit(struct proposer_state *state, char* msg, int msg_size) {
    Message m;
    initialize_message(&m, phase2a);
    memcpy(m.paxosval, msg, msg_size);
    m.paxosval[msg_size] = '\0';
    m.client = *state->mine;
    pack(&m);
    paxos_send(state, (char*)&m, sizeof(m));

    // socklen_t serverlen = sizeof(*state->dest);
    // int n = sendto(state->sock, &m, sizeof(Message), 0, (struct sockaddr*) state->dest, serverlen);
    // if (n < 0) {
    //     perror("ERROR in sendto");
    //     return;
    // }
}

void set_application_ctx(struct proposer_state *state, void *arg) {
    state->app_ctx = arg;
}

void register_callback(struct proposer_state *state, deliver_fn res_cb) {
    state->deliver = res_cb;
}

struct proposer_state* proposer_state_new(Config *conf) {
    struct proposer_state *state = malloc(sizeof(struct proposer_state));
    state->conf = conf;
    state->base = event_base_new();
    return state;
}

int init_proposer(struct proposer_state *state, char* interface) {
    struct sockaddr_in *coordinator = malloc(sizeof (struct sockaddr_in));
    state->mine = malloc(sizeof (struct sockaddr_in));
    socklen_t len = sizeof(struct sockaddr_in);
    state->rawsock = create_rawsock();
    int sock = create_server_socket(0);
    if (sock < 0) {
        perror("cannot create socket");
        return EXIT_FAILURE;
    }
    state->sock = sock;
    struct hostent *server = gethostbyname(state->conf->coordinator_addr);
    if (server == NULL) {
        fprintf(stderr, "ERROR, no such host as %s\n", state->conf->coordinator_addr);
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
    coordinator->sin_port = htons(state->conf->coordinator_port);
    state->dest = coordinator;
    init_rawsock(state, state->mine, state->dest);

    return 0;
}


void free_proposer(struct proposer_state *state) {
    free(state->dest);
    free(state->mine);
    event_free(state->ev_recv);
    event_free(state->ev_sigterm);
    event_free(state->ev_sigint);
    event_base_free(state->base);
    free(state->conf);
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
    printf("second %d, microsecond %d\n", conf->second, conf->microsecond);

    struct timeval timeout = {conf->second, conf->microsecond};
    state->ev_recv = event_new(state->base, state->sock, EV_READ|EV_PERSIST|EV_TIMEOUT, on_response, state);
    state->ev_sigterm = evsignal_new(state->base, SIGTERM, signal_handler, state);
    state->ev_sigint = evsignal_new(state->base, SIGINT, signal_handler, state);
    event_add(state->ev_recv, &timeout);
    event_add(state->ev_sigint, NULL);
    event_add(state->ev_sigterm, NULL);
    return state;
}

