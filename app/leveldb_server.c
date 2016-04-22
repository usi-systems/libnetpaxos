/* For sockaddr_in */
#include <netinet/in.h>
/* For socket functions */
#include <sys/socket.h>
/* For fcntl */
#include <fcntl.h>
/* For libevent */
#include <event2/event.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
/* For LevelDB */
#include <leveldb/c.h>

#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include "learner.h"
#include "application.h"

#define MAX_LINE 16384

void do_read(evutil_socket_t fd, short events, void *arg);
void do_write(evutil_socket_t fd, short events, void *arg);

struct application {
    struct event_base *base;
    leveldb_t *db;
    leveldb_options_t *options;
    leveldb_readoptions_t *roptions;
    leveldb_writeoptions_t *woptions;
    int mps;
} application;

struct application* application_new() {
    struct application *ctx = malloc(sizeof(struct application));
    char *err = NULL;
    /******************************************/
    /* OPEN DB */
    ctx->options = leveldb_options_create();
    leveldb_options_set_create_if_missing(ctx->options, 1);
    ctx->db = leveldb_open(ctx->options, "/tmp/ycsb", &err);
    if (err != NULL) {
      fprintf(stderr, "Open fail.\n");
      exit(EXIT_FAILURE);
    }
    /* reset error var */
    leveldb_free(err); err = NULL;
    ctx->woptions = leveldb_writeoptions_create();
    ctx->roptions = leveldb_readoptions_create();

    return ctx;
}

void application_free(struct application *ctx) {
    char *err = NULL;
    event_base_free(ctx->base);
    leveldb_close(ctx->db);
    leveldb_destroy_db(ctx->options, "/tmp/ycsb", &err);
    if (err != NULL) {
      fprintf(stderr, "Destroy fail.\n");
      return;
    }
    leveldb_free(err); err = NULL;
    leveldb_options_destroy(ctx->options);
    leveldb_writeoptions_destroy(ctx->woptions);
    leveldb_readoptions_destroy(ctx->roptions);
    free(ctx);
}



int deliver(const char* request, void *arg, char **return_val, int *return_vsize) {
    struct application *state = arg;
    if (!request || request[0] == '\0') {
        return FAILED;
    }
    char *err = NULL;
    char op = request[0];
    size_t read_len;
    switch(op) {
        case PUT: {
            unsigned char ksize = request[1];
            unsigned char vsize = request[2];
            leveldb_put(state->db, state->woptions, &request[3], ksize, &request[3+ksize], vsize, &err);
            if (err != NULL) {
                leveldb_free(err); err = NULL;
                return FAILED;
            }
            return SUCCESS;
        }
        case GET: {
            unsigned char ksize = request[1];
             *return_val = leveldb_get(state->db, state->roptions, &request[3], ksize, &read_len, &err);
            if (err != NULL) {
                leveldb_free(err); err = NULL;
                return FAILED;
            }
            if (*return_val) {
                *return_vsize = read_len;
                return GOT_VALUE;
            }
            return NOT_FOUND;
        }
        case DELETE: {
            unsigned char ksize = request[1];
            leveldb_delete(state->db, state->woptions, &request[3], ksize, &err);
            if (err != NULL) {
                leveldb_free(err); err = NULL;
                return FAILED;
            }
            return SUCCESS;
        }
    }
    return INVALID_OP;
}

void
readcb(struct bufferevent *bev, void *arg)
{
    struct application *ctx = arg;
    char request[32];
    memset(request, 0, 32);
    struct evbuffer *input, *output;
    input = bufferevent_get_input(bev);
    output = bufferevent_get_output(bev);

    int n = evbuffer_remove(input, request, sizeof(request));
    // printf("received: %s, size: %d\n", request, n);
    char *value;
    int vsize;
    int res = deliver(request, ctx, &value, &vsize);
    ctx->mps++;
    switch(res) {
        case SUCCESS:
            evbuffer_add(output, "SUCCESS", 8);
            break;
        case GOT_VALUE: {
            evbuffer_add(output, value, vsize);
            free(value);
            break;
        }
        case NOT_FOUND:
            evbuffer_add(output, "NOT_FOUND", 10);
            break;
    }
}

void
errorcb(struct bufferevent *bev, short error, void *ctx)
{
    if (error & BEV_EVENT_EOF) {
        /* connection has been closed, do any clean up here */
        /* ... */
    } else if (error & BEV_EVENT_ERROR) {
        /* check errno to see what error occurred */
        /* ... */
    } else if (error & BEV_EVENT_TIMEOUT) {
        /* must be a timeout event handle, handle it */
        /* ... */
    }
    bufferevent_free(bev);
}

void
do_accept(evutil_socket_t listener, short event, void *arg)
{
    struct application *ctx = arg;
    struct sockaddr_storage ss;
    socklen_t slen = sizeof(ss);
    int fd = accept(listener, (struct sockaddr*)&ss, &slen);
    if (fd < 0) {
        perror("accept");
    } else if (fd > FD_SETSIZE) {
        close(fd);
    } else {
        struct bufferevent *bev;
        evutil_make_socket_nonblocking(fd);
        bev = bufferevent_socket_new(ctx->base, fd, BEV_OPT_CLOSE_ON_FREE);
        bufferevent_setcb(bev, readcb, NULL, errorcb, ctx);
        bufferevent_setwatermark(bev, EV_READ, 0, MAX_LINE);
        bufferevent_enable(bev, EV_READ|EV_WRITE);
    }
}

void do_monitor(evutil_socket_t fd, short what, void *arg) {
    struct application *ctx = arg;
    if ( ctx->mps ) {
        fprintf(stdout, "%d\n", ctx->mps);
    }
    ctx->mps = 0;
}

void
run(int port)
{
    evutil_socket_t listener;
    struct sockaddr_in sin;
    struct event *listener_event;
    struct event *monitor_event;
    struct application *ctx = application_new();

    ctx->mps = 0;
    ctx->base = event_base_new();

    if (!ctx->base)
        return; /*XXXerr*/

    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = 0;
    sin.sin_port = htons(port);

    listener = socket(AF_INET, SOCK_STREAM, 0);
    evutil_make_socket_nonblocking(listener);

#ifndef WIN32
    {
        int one = 1;
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    }
#endif

    if (bind(listener, (struct sockaddr*)&sin, sizeof(sin)) < 0) {
        perror("bind");
        return;
    }

    if (listen(listener, 16)<0) {
        perror("listen");
        return;
    }

    listener_event = event_new(ctx->base, listener, EV_READ|EV_PERSIST, do_accept, ctx);
    event_add(listener_event, NULL);

    monitor_event = event_new(ctx->base, -1, EV_TIMEOUT|EV_PERSIST, do_monitor, ctx);
    struct timeval one_second = {1, 0};
    event_add(monitor_event, &one_second);

    event_base_priority_init(ctx->base, 2);
    event_priority_set(monitor_event, 0);
    event_priority_set(listener_event, 1);

    // Comment the line below for valgrind check
    event_base_dispatch(ctx->base);
    event_free(listener_event);
    event_free(monitor_event);
    application_free(ctx);
}

int
main(int argc, char **argv)
{
    if (argc < 2) {
        printf("%s port\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    int port = atoi(argv[1]);
    setvbuf(stdout, NULL, _IONBF, 0);

    run(port);
    return EXIT_SUCCESS;
}