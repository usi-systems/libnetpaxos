#include <event2/event.h>
#include <event2/bufferevent.h>
#include <sys/socket.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

char *msg[] = { "PUT key val", "GET key", "DEL key", "GET not"};

struct info {
    struct event_base *base;
    int mps;
    int total_commands;
    char **msg;
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
        bufferevent_enable(bev, EV_READ|EV_WRITE);
        submit(bev, msg[0], 12);
    } else if (events & (BEV_EVENT_ERROR|BEV_EVENT_EOF)) {
         struct info *inf = ptr;
         if (events & BEV_EVENT_ERROR) {
                 int err = bufferevent_socket_get_dns_error(bev);
                 if (err)
                         printf("DNS error: %s\n", evutil_gai_strerror(err));
         }
         printf("Closing\n");
         event_base_loopexit(inf->base, NULL);
    }
}

void read_input(struct info *inf, char* workload) {
    char * line = NULL;
    size_t len = 0;
    ssize_t read;
    FILE *fp = fopen(workload, "r");
    if (fp == NULL)
        exit(EXIT_FAILURE);

    while ((read = getline(&line, &len, fp)) != -1) {
        printf("Retrieved line of length %zu :\n", read);
        printf("%s", line);
    }
    fclose(fp);
}

struct info *info_new() {
    struct info *inf = malloc(sizeof(struct info));
    inf->mps = 0;
    inf->total_commands = 0;
}

void info_free(struct info *inf) {
    event_base_free(inf->base);
    free(inf);
}


int main(int argc, char* argv[])
{
    if (argc < 4) {
        printf("Usage: %s address port workload\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    struct info *inf = info_new();

    struct evdns_base *dns_base;
    struct bufferevent *bev;
    struct event *ev_monitor;

    inf->base = event_base_new();
    dns_base = evdns_base_new(inf->base, 1);

    int port = atoi(argv[2]);

    bev = bufferevent_socket_new(inf->base, -1, BEV_OPT_CLOSE_ON_FREE);
    bufferevent_setcb(bev, readcb, NULL, eventcb, inf);

    if (bufferevent_socket_connect_hostname(bev, dns_base, AF_INET, argv[1], port) < 0) {
        perror("connect failed.\n");
    }

    ev_monitor = event_new(inf->base, -1, EV_TIMEOUT|EV_PERSIST, monitor, inf);
    struct timeval one_second = {1, 0};
    event_add(ev_monitor, &one_second);

    read_input(inf, argv[3]);

    event_base_dispatch(inf->base);
    bufferevent_free(bev);
    info_free(inf);
    return 0;
}
