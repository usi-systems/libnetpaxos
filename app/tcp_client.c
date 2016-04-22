#include <event2/event.h>
#include <event2/bufferevent.h>
#include <sys/socket.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "application.h"

#define BUFSIZE 128
struct info {
    struct event_base *base;
    int mps;
    int count;
    int num_elements;
    struct timespec start;
    char **msg;
    FILE *fp;
};

void monitor(evutil_socket_t fd, short what, void *arg) {
    struct info *inf = arg;
    if ( inf->mps ) {
        fprintf(stdout, "%d\n", inf->mps);
    }
    inf->mps = 0;
}

void submit(struct bufferevent *bev, char* req) {
    char msg[BUFSIZE];
    char* replica = strdup(req);
    char* token = strtok(replica, " ");
    int size;
    if (strcmp(token, "PUT") == 0) {
        char *key = strtok(NULL, " ");
        char *value = strtok(NULL, " ");
        msg[0] = PUT;
        msg[1] = (unsigned char) strlen(key);
        msg[2] = (unsigned char) strlen(value);
        memcpy(&msg[3], key, msg[1]);
        memcpy(&msg[3+msg[1]], value, msg[2]);
        size = msg[1] + msg[2] + 4; // 3 for three chars and 1 for terminator
    }
    else if (strcmp(token, "GET") == 0) {
        char *key = strtok(NULL, " ");
        msg[0] = GET;
        msg[1] = (unsigned char) strlen(key);
        msg[2] = 1;
        memcpy(&msg[3], key, msg[1]);
        size = msg[1] + 4; // 3 for three chars and 1 for terminator
    }
    else if (strcmp(token, "DEL") == 0) {
        char *key = strtok(NULL, " ");
        msg[0] = DELETE;
        msg[1] = (unsigned char) strlen(key);
        msg[2] = 1;
        memcpy(&msg[3], key, msg[1]);
        size = msg[1] + 4; // 3 for three chars and 1 for terminator
    }
    bufferevent_write(bev, msg, size);
    free(replica);
    bzero(msg, BUFSIZE);
}

int timediff(struct timespec *result, struct timespec *end, struct timespec *start)
{
  result->tv_sec = end->tv_sec - start->tv_sec;
  result->tv_nsec = end->tv_nsec - start->tv_nsec;

  /* Return 1 if result is negative. */
  return end->tv_sec < start->tv_sec;
}

void readcb(struct bufferevent *bev, void *ptr)
{
    struct timespec result, end;
    clock_gettime(CLOCK_REALTIME, &end);
    struct info *inf = ptr;
    timediff(&result, &end, &inf->start);
    char buf[BUFSIZE];
    int n;
    struct evbuffer *input = bufferevent_get_input(bev);
    int negative = timediff(&result, &end, &inf->start);
    if (negative) {
        fprintf(stderr, "Latency is negative\n");
    } else {
        double latency = (result.tv_sec + ((double)result.tv_nsec) / 1e9);
        fprintf(inf->fp, "%.9f\n", latency);
    }

    while ((n = evbuffer_remove(input, buf, sizeof(buf))) > 0) {
        // printf("%s\n", buf);
    }
    inf->mps++;
    int idx = inf->mps % inf->count;
    submit(bev, inf->msg[idx]);
    clock_gettime(CLOCK_REALTIME, &inf->start);
    bzero(buf, BUFSIZE);
}

void eventcb(struct bufferevent *bev, short events, void *ptr)
{
    struct info *inf = ptr;
    if (events & BEV_EVENT_CONNECTED) {
        printf("Connect okay.\n");
        bufferevent_enable(bev, EV_READ|EV_WRITE);

        submit(bev, inf->msg[0]);
        clock_gettime(CLOCK_REALTIME, &inf->start);

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

void remove_newline(char *line) {
    char *pos;
    if ((pos=strchr(line, '\n')) != NULL)
        *pos = '\0';
}

void read_input(struct info *inf, char* workload) {
    char * line = NULL;
    size_t len = 0;
    ssize_t read;
    FILE *fp = fopen(workload, "r");
    if (fp == NULL)
        exit(EXIT_FAILURE);

    while ((read = getline(&line, &len, fp)) != -1) {
        if (read == 1 || line[0] == '\0')
            break;
        if (inf->count >= inf->num_elements) {
            inf->num_elements += 10;
            inf->msg = realloc(inf->msg, inf->num_elements);
        }
        remove_newline(line);
        inf->msg[inf->count] = strdup(line);
        inf->count++;
    }
    if (line)
        free(line);
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
    fclose(inf->fp);
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
    if (argc < 5) {
        printf("Usage: %s address port workload output\n", argv[0]);
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
    inf->fp = fopen(argv[4], "w+");

    event_base_dispatch(inf->base);
    bufferevent_free(bev);
    info_free(inf);
    return 0;
}
