#include <event2/event.h>
#include <event2/bufferevent.h>
#include <sys/socket.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

char *msg[] = { "PUT key val", "GET key", "DEL key", "GET not"};

struct info {
    int mps;
};

void monitor(evutil_socket_t fd, short what, void *arg) {
    struct info *inf = arg;
    if ( inf->mps ) {
        fprintf(stdout, "%d\n", inf->mps);
    }
    inf->mps = 0;
}

void submit(struct bufferevent *bev, char* req, int size) {
    bufferevent_write(bev, req, size);
}


void readcb(struct bufferevent *bev, void *ptr)
{
    struct info *inf = ptr;
    char buf[1024];
    int n;
    struct evbuffer *input = bufferevent_get_input(bev);
    while ((n = evbuffer_remove(input, buf, sizeof(buf))) > 0) {
        // fwrite(buf, 1, n, stdout);
    }
    inf->mps++;
    submit(bev, msg[0], 12);
}

void eventcb(struct bufferevent *bev, short events, void *ptr)
{
    if (events & BEV_EVENT_CONNECTED) {
         printf("Connect okay.\n");
         submit(bev, msg[0], 12);
    } else if (events & (BEV_EVENT_ERROR|BEV_EVENT_EOF)) {
         struct event_base *base = ptr;
         if (events & BEV_EVENT_ERROR) {
                 int err = bufferevent_socket_get_dns_error(bev);
                 if (err)
                         printf("DNS error: %s\n", evutil_gai_strerror(err));
         }
         printf("Closing\n");
         bufferevent_free(bev);
         event_base_loopexit(base, NULL);
    }
}

int main(int argc, char* argv[])
{
    if (argc < 3) {
        printf("Usage: %s address port\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    struct info *inf = malloc(sizeof(struct info));
    inf->mps = 0;

    struct event_base *base;
    struct evdns_base *dns_base;
    struct bufferevent *bev;
    struct event *ev_monitor;
    struct sockaddr_in sin;

    base = event_base_new();
    dns_base = evdns_base_new(base, 1);

    int port = atoi(argv[2]);

    bev = bufferevent_socket_new(base, -1, BEV_OPT_CLOSE_ON_FREE);

    bufferevent_setcb(bev, readcb, NULL, eventcb, inf);
    bufferevent_enable(bev, EV_READ|EV_WRITE);
    if (bufferevent_socket_connect_hostname(bev, dns_base, AF_INET, argv[1], port) < 0) {
        bufferevent_free(bev);
        return -1;
    }

    ev_monitor = event_new(base, -1, EV_TIMEOUT|EV_PERSIST, monitor, inf);
    struct timeval one_second = {1, 0};
    event_add(ev_monitor, &one_second);

    event_base_dispatch(base);
    return 0;
}
