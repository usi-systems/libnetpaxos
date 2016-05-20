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
#include "message.h"
#include "netpaxos_utils.h"
#include "config.h"
#include "coordinator.h"
#include <netinet/udp.h>   //Provides declarations for udp header
#include <netinet/ip.h>    //Provides declarations for ip header

void propose_value(CoordinatorCtx *ctx, void *arg, int size, struct sockaddr_in client);

CoordinatorCtx *coordinator_new(Config conf) {
    CoordinatorCtx *ctx = malloc(sizeof(CoordinatorCtx));
    ctx->conf = conf;
    ctx->cur_inst = 0;
    ctx->dest = malloc( sizeof (struct sockaddr_in));
    ctx->mine = malloc( sizeof (struct sockaddr_in));
    ctx->msg_in = malloc(sizeof(struct Message));
    return ctx;
}

void coordinator_free(CoordinatorCtx *ctx) {
    event_base_free(ctx->base);
    free(ctx->msg_in);
    free(ctx->mine);
    free(ctx->dest);
    free(ctx);

}


void init_coord_rawsock (struct CoordinatorCtx *ctx, struct sockaddr_in *mine, struct sockaddr_in *dest)
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


void signal_handler(evutil_socket_t fd, short what, void *arg) {
    CoordinatorCtx *ctx = arg;
    if (what&EV_SIGNAL) {
        printf("Stop Coordinator\n");
        event_base_loopbreak(ctx->base);
    }
}


int send_message (struct CoordinatorCtx *ctx, char *msg, int msglen) {
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
    return 0;
}


void on_value(evutil_socket_t fd, short what, void *arg) {
    CoordinatorCtx *ctx = (CoordinatorCtx *) arg;
    struct sockaddr_in remote;
    socklen_t remote_len = sizeof(remote);
    size_t msg_size = sizeof(Message);
    int n = recvfrom(fd, ctx->msg_in, msg_size, 0, (struct sockaddr *) &remote, &remote_len);
    if (n < 0) {
      perror("ERROR in recvfrom");
      return;
    }
    int *inst = (int *)ctx->msg_in;
    *inst = htonl(ctx->cur_inst);
    if (ctx->conf.verbose) {
        print_message(ctx->msg_in);
    }

    send_message(ctx, (char *)ctx->msg_in, msg_size);
    ctx->cur_inst++;
}



int start_coordinator(Config *conf) {
    CoordinatorCtx *ctx = coordinator_new(*conf);
    ctx->base = event_base_new();
    struct hostent *server;

    server = gethostbyname(conf->acceptor_addr);
    if (server == NULL) {
        fprintf(stderr, "ERROR, no such host as %s\n", conf->acceptor_addr);
        return EXIT_FAILURE;
    }

    /* build the server's Internet address */
    bzero((char *) ctx->dest, sizeof(struct sockaddr_in));
    ctx->dest->sin_family = AF_INET;
    bcopy((char *)server->h_addr,
      (char *)&(ctx->dest->sin_addr.s_addr), server->h_length);
    ctx->dest->sin_port = htons(conf->acceptor_port);

    int listen_socket = create_server_socket(conf->coordinator_port);
    if (net_ip__is_multicast_ip(conf->coordinator_addr)) {
        addMembership(conf->coordinator_addr, listen_socket);
    }
    ctx->sock = listen_socket;

    socklen_t len = sizeof(struct sockaddr_in);
    if (getsockname(ctx->sock, (struct sockaddr *)ctx->mine, &len) == -1) {
        perror("getsockname");
        exit(EXIT_FAILURE);
    }

    ctx->rawsock = create_rawsock();
    init_coord_rawsock(ctx, ctx->mine, ctx->dest);

    struct event *ev_recv;
    ev_recv = event_new(ctx->base, ctx->sock, EV_READ|EV_PERSIST, on_value, ctx);
    event_add(ev_recv, NULL);

    struct event *ev_sigterm;
    ev_sigterm = evsignal_new(ctx->base, SIGTERM, signal_handler, ctx);
    event_add(ev_sigterm, NULL);

    struct event *ev_sigint;
    ev_sigint = evsignal_new(ctx->base, SIGINT, signal_handler, ctx);
    event_add(ev_sigint, NULL);

    event_base_dispatch(ctx->base);
    event_free(ev_recv);
    event_free(ev_sigterm);
    event_free(ev_sigint);
    coordinator_free(ctx);

    close(listen_socket);
    return EXIT_SUCCESS;
}

