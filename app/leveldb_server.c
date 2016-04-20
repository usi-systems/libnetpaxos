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

void application_free(struct application *state) {
    char *err = NULL;
    leveldb_close(state->db);
    leveldb_destroy_db(state->options, "/tmp/ycsb", &err);
    if (err != NULL) {
      fprintf(stderr, "Destroy fail.\n");
      return;
    }
    leveldb_free(err); err = NULL;

}



void *deliver(const char* chosen, void *arg) {
    struct application *state = (struct application *)arg;
    // printf("delivered %s\n", chosen);
    if (!chosen || chosen[0] == '\0') {
        return NULL;
    }
    char *err = NULL;
    char *chosen_duplicate = strdup(chosen);
    char* token = strtok(chosen_duplicate, " ");
    size_t read_len;
    if (strcmp(token, "PUT") == 0) {
        char *key = strtok(NULL, " ");
        if (!key) {
            free(chosen_duplicate);
            return strdup("Key is NULL");
        }
        char *val = strtok(NULL, " ");
        if (!val) {
            free(chosen_duplicate);
            return strdup("Value is NULL");
        }
        size_t keylen = strlen(key) + 1;
        size_t vallen = strlen(val) + 1;
        // printf("PUT (%s:%zu, %s:%zu)\n", key, keylen, val, vallen);
        leveldb_put(state->db, state->woptions, key, keylen, val, vallen, &err);
        if (err != NULL) {
            free(chosen_duplicate);
            return err;
        }
        leveldb_free(err); err = NULL;
        return strdup("PUT OK");
    }
    else if (strcmp(token, "GET") == 0) {
        char *key = strtok(NULL, " ");
        if (!key) {
            free(chosen_duplicate);
            return strdup("Key is NULL");
        }
        int keylen = strlen(key) + 1;
        // printf("PUT (%s:%zu)\n", key, keylen);
        char *val = leveldb_get(state->db, state->roptions, key, keylen, &read_len, &err);
        if (err != NULL) {
            free(chosen_duplicate);
            return err;
        }
        leveldb_free(err); err = NULL;
        // printf("%s: %s\n", key, val);
        if (!val) {
            free(chosen_duplicate);
            return strdup("NOT FOUND");
        }
        return val;
    }
    else if (strcmp(token, "DEL") == 0) {
        char *key = strtok(NULL, " ");
        if (!key) {
            free(chosen_duplicate);
            return strdup("Key is NULL");
        }
        int keylen = strlen(key) + 1;
        leveldb_delete(state->db, state->woptions, key, keylen, &err);
        if (err != NULL) {
            free(chosen_duplicate);
            return err;
        }
        leveldb_free(err); err = NULL;
        free(chosen_duplicate);
        return strdup("DELETE OK");
    }
    return NULL;
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
    char *res = deliver(request, ctx);
    if (res) {
        // printf("sent: %s, size: %zu\n", res, strlen(res) + 1);
        evbuffer_add(output, res, strlen(res) + 1);
        free(res);
        ctx->mps++;
    }
    // else {
    //     evbuffer_add(output, "NULL", 5);
    // }
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
    event_base_dispatch(ctx->base);
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