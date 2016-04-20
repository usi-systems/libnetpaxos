#include <event2/event.h>
#include <event2/bufferevent.h>
#include <sys/socket.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

struct info {
    struct event_base *base;
    int mps;
    int count;
    int num_elements;
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
    int idx = inf->mps % inf->count;
    char *msg = inf->msg[idx];
    int size = strlen(inf->msg[idx] + 1);
    printf("msg[%d]: %s, size:%d\n", idx, msg, size);
    submit(bev, msg, size);
}

void eventcb(struct bufferevent *bev, short events, void *ptr)
{
    struct info *inf = ptr;
    if (events & BEV_EVENT_CONNECTED) {
        printf("Connect okay.\n");
        bufferevent_enable(bev, EV_READ|EV_WRITE);

        int idx = inf->mps % inf->count;
        char *msg = inf->msg[idx];
        int size = strlen(inf->msg[idx] + 1);
        printf("sizeof msg: %d\n", size);
        submit(bev, msg, size);

    } else if (events & (BEV_EVENT_ERROR|BEV_EVENT_EOF)) {
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
        if (inf->count >= inf->num_elements) {
            inf->num_elements += 10;
            inf->msg = realloc(inf->msg, inf->num_elements);
        }
        inf->msg[inf->count] = malloc(read + 1);
        strcpy(inf->msg[inf->count], line);
        free(line);
        inf->count++;
    }
    fclose(fp);
}

struct info *info_new() {
    struct info *inf = malloc(sizeof(struct info));
    inf->mps = 0;
    inf->num_elements = 10;
    // Initialize to 10 elements
    inf->msg = calloc(inf->num_elements, sizeof(char));
}

void info_free(struct info *inf) {
    event_base_free(inf->base);
    int i;
    for (i = 0; i < inf->count; i++) {
        if(inf->msg[i])
            free(inf->msg[i]);
    }
    free(inf->msg);
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
