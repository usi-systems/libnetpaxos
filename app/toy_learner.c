#define _GNU_SOURCE
#include <stdio.h>
#include <event2/event.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <event2/event.h>
#include <string.h>
#include <strings.h>
#include <inttypes.h>
#include <signal.h>
#include <math.h>
#include <errno.h>
#include <stdlib.h>
#include "message.h"
#include "learner.h"
#include "netpaxos_utils.h"
#include "config.h"
#define _GNU_SOURCE
#include <leveldb/c.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "application.h"

struct application {
    leveldb_t *db;
    leveldb_options_t *options;
    leveldb_readoptions_t *roptions;
    leveldb_writeoptions_t *woptions;
} application;

struct application* application_new() {
    struct application *state = malloc(sizeof(struct application));
    char *err = NULL;
    /******************************************/
    /* OPEN DB */
    state->options = leveldb_options_create();
    leveldb_options_set_create_if_missing(state->options, 1);
    state->db = leveldb_open(state->options, "/tmp/testdb", &err);
    if (err != NULL) {
      fprintf(stderr, "Open fail.\n");
      exit(EXIT_FAILURE);
    }
    /* reset error var */
    leveldb_free(err); err = NULL;
    state->woptions = leveldb_writeoptions_create();
    state->roptions = leveldb_readoptions_create();

    return state;
}

void application_destroy(struct application *state) {
    // char *err = NULL;
    leveldb_close(state->db);
    // leveldb_destroy_db(state->options, "/tmp/testdb", &err);
    // if (err != NULL) {
    //   fprintf(stderr, "Destroy fail.\n");
    //   return;
    // }
    // leveldb_free(err); err = NULL;

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
                // printf("Return: %s\n", *return_val);
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


int create_server_socket(int port);
LearnerCtx *learner_ctx_new(Config conf);
void learner_ctx_destroy(LearnerCtx *st);

/* Here's a callback function that calls loop break */
LearnerCtx *learner_ctx_new(Config conf) {
    LearnerCtx *ctx = malloc(sizeof(LearnerCtx));
    ctx->base = event_base_new();
    ctx->conf = conf;
    ctx->mps = 0;
    ctx->res_idx = 0;
    ctx->num_packets = 0;
    double maj = ((double)(conf.num_acceptors + 1)) / 2;
    ctx->maj = ceil(maj);
    ctx->states = calloc(ctx->conf.maxinst, sizeof(paxos_state*));
    int i;
    for (i = 0; i < ctx->conf.maxinst; i++) {
        ctx->states[i] = malloc(sizeof(paxos_state));
        ctx->states[i]->rnd = 0;
        ctx->states[i]->from = 0;
        ctx->states[i]->count = 0;
        ctx->states[i]->finished = 0;
        ctx->states[i]->paxosval = malloc(PAXOS_VALUE_SIZE);
        bzero(ctx->states[i]->paxosval, PAXOS_VALUE_SIZE);
    }
    // ctx->fp = fopen(fname, "w+");
    ctx->msgs = calloc(ctx->conf.vlen, sizeof(struct mmsghdr));
    ctx->iovecs = calloc(ctx->conf.vlen, sizeof(struct iovec));
    ctx->out_msgs = calloc(ctx->conf.vlen, sizeof(struct mmsghdr));
    ctx->out_iovecs = calloc(ctx->conf.vlen, sizeof(struct iovec));
    ctx->out_bufs = calloc(ctx->conf.vlen, sizeof(struct Message));
    ctx->bufs = calloc(ctx->conf.vlen, sizeof(struct Message));
    ctx->res_bufs = calloc(ctx->conf.vlen, sizeof(char*));
    for (i = 0; i < ctx->conf.vlen; i++) {
        ctx->res_bufs[i] = malloc(BUFSIZE + 1);
    }
    for (i = 0; i < ctx->conf.vlen; i++) {
        ctx->iovecs[i].iov_base          = &ctx->bufs[i];
        ctx->iovecs[i].iov_len           = sizeof(struct Message);
        ctx->msgs[i].msg_hdr.msg_iov     = &ctx->iovecs[i];
        ctx->msgs[i].msg_hdr.msg_iovlen  = 1;
    }
    return ctx;
}

void learner_ctx_destroy(LearnerCtx *ctx) {
    int i;
    // fclose(ctx->fp);
    event_base_free(ctx->base);
    for (i = 0; i < ctx->conf.maxinst; i++) {
        free(ctx->states[i]->paxosval);
        free(ctx->states[i]);
    }
    free(ctx->states);

    free(ctx->msgs);
    free(ctx->iovecs);
    free(ctx->bufs);
    free(ctx->out_msgs);
    free(ctx->out_iovecs);
    free(ctx->out_bufs);
    for (i = 0; i < ctx->conf.vlen; i++) {
        free(ctx->res_bufs[i]);
    }
    free(ctx->res_bufs);
    free(ctx);
}

void signal_handler(evutil_socket_t fd, short what, void *arg) {
    LearnerCtx *ctx = (LearnerCtx *) arg;
    if (what&EV_SIGNAL) {
        printf("Stop learner\n");
        event_base_loopbreak(ctx->base);
        // disable for now
        // int i;
        // for (i = 0; i < ctx->conf.maxinst; i++) {
        //     fprintf(ctx->fp, "%s\n", ctx->states[i]->paxosval);
        // }
        // fprintf(stdout, "num_packets: %d\n", ctx->num_packets);
    }
}

void monitor(evutil_socket_t fd, short what, void *arg) {
    LearnerCtx *ctx = (LearnerCtx *) arg;
    if ( ctx->mps ) {
        fprintf(stdout, "%d\n", ctx->mps);
    }
    ctx->mps = 0;
}


void handle_accepted(LearnerCtx *ctx, Message *msg, evutil_socket_t fd) {
    paxos_state *state = ctx->states[msg->inst];
    if (msg->rnd == state->rnd) {
        int mask = 1 << msg->acptid;
        state->from = state->from | mask;
        state->count++;
        if (!state->paxosval[0]) {
            strcpy(state->paxosval, msg->paxosval);
        }
        // printf("instance: %d - count %d\n", msg->inst, state->count);
        if (state->count >= ctx->maj) { // Chosen value
            state->finished = 1;        // Marked values has been chosen
            // printf("deliver %d\n", msg->inst);
            char *value;
            int vsize;
            int res = ctx->deliver(state->paxosval, ctx->app, &value, &vsize);
            ctx->mps++;
            ctx->num_packets++;
            switch(res) {
                case SUCCESS: {
                    ctx->res_bufs[ctx->res_idx][0] = SUCCESS;
                    ctx->out_iovecs[ctx->res_idx].iov_base         = (void *)ctx->res_bufs[ctx->res_idx];
                    ctx->out_iovecs[ctx->res_idx].iov_len          = 1;
                    ctx->out_msgs[ctx->res_idx].msg_hdr.msg_name    = &msg->client;
                    ctx->out_msgs[ctx->res_idx].msg_hdr.msg_namelen = sizeof(struct sockaddr_in);
                    ctx->out_msgs[ctx->res_idx].msg_hdr.msg_iov    = &ctx->out_iovecs[ctx->res_idx];
                    ctx->out_msgs[ctx->res_idx].msg_hdr.msg_iovlen = 1;
                    ctx->res_idx++;
                    break;
                    }
                case GOT_VALUE: {
                    memcpy(ctx->res_bufs[ctx->res_idx], value, vsize);
                    ctx->out_iovecs[ctx->res_idx].iov_base         = (void *)ctx->res_bufs[ctx->res_idx];
                    ctx->out_iovecs[ctx->res_idx].iov_len          = vsize;
                    ctx->out_msgs[ctx->res_idx].msg_hdr.msg_name    = &msg->client;
                    ctx->out_msgs[ctx->res_idx].msg_hdr.msg_namelen = sizeof(struct sockaddr_in);
                    ctx->out_msgs[ctx->res_idx].msg_hdr.msg_iov    = &ctx->out_iovecs[ctx->res_idx];
                    ctx->out_msgs[ctx->res_idx].msg_hdr.msg_iovlen = 1;
                    // printf("In handle_accepted %s\n", ctx->res_bufs[0]);
                    ctx->res_idx++;
                    free(value);
                    break;
                }
                case NOT_FOUND: {
                    ctx->res_bufs[ctx->res_idx][0] = NOT_FOUND;
                    ctx->out_iovecs[ctx->res_idx].iov_base         = (void *)ctx->res_bufs[ctx->res_idx];
                    ctx->out_iovecs[ctx->res_idx].iov_len          = 1;
                    ctx->out_msgs[ctx->res_idx].msg_hdr.msg_name    = &msg->client;
                    ctx->out_msgs[ctx->res_idx].msg_hdr.msg_namelen = sizeof(struct sockaddr_in);
                    ctx->out_msgs[ctx->res_idx].msg_hdr.msg_iov    = &ctx->out_iovecs[ctx->res_idx];
                    ctx->out_msgs[ctx->res_idx].msg_hdr.msg_iovlen = 1;
                    ctx->res_idx++;
                    break;
                }
            }
        }
    } else if (msg->rnd > state->rnd) {
        state->rnd = msg->rnd;
        int mask = 1 << msg->acptid;
        state->from = state->from | mask;
        state->count = 1;
        strcpy(state->paxosval, msg->paxosval);
    }
}


void on_value(evutil_socket_t fd, short what, void *arg)
{
    LearnerCtx *ctx = arg;
    int retval = recvmmsg(ctx->sock, ctx->msgs, ctx->conf.vlen, MSG_WAITFORONE, NULL);
    if (retval < 0) {
      perror("recvmmsg()");
      exit(EXIT_FAILURE);
    }
    else if (retval > 0) {
        int i;
        for (i = 0; i < retval; i++) {
            ctx->out_bufs[i] = ctx->bufs[i];
            unpack(&ctx->out_bufs[i]);
            if (ctx->conf.verbose) {
                printf("client info %s:%d\n",
                    inet_ntoa(ctx->out_bufs[i].client.sin_addr),
                    ntohs(ctx->out_bufs[i].client.sin_port));
                printf("received %d messages\n", retval);
                print_message(&ctx->out_bufs[i]);
            }
            if (ctx->out_bufs[i].inst > (unsigned int) ctx->conf.maxinst) {
                if (ctx->conf.verbose) {
                    fprintf(stderr, "State Overflow\n");
                }
                return;
            }
            handle_accepted(ctx, &ctx->out_bufs[i], ctx->sock);
        }
        if (ctx->res_idx) {
            int r = sendmmsg(ctx->sock, ctx->out_msgs, ctx->res_idx, 0);
            // printf("GOT value %s\n", ctx->res_bufs[0]);
            if (r <= 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
                }

                if (errno == ECONNREFUSED) {
                }
                perror("sendmmsg()");
             }
            if (ctx->conf.verbose) {
                printf("Send %d messages\n", r);
            }
            ctx->res_idx = 0;
        }
    }
}


int start_learner(Config *conf, int (*deliver_cb)(const char* req, void* arg, char **value, int *vsize), void* arg) {
    LearnerCtx *ctx = learner_ctx_new(*conf);
    ctx->app = arg;
    ctx->deliver = deliver_cb;
    int server_socket = create_server_socket(conf->learner_port);
    addMembership(conf->learner_addr, server_socket);
    ctx->sock = server_socket;

    struct timeval timeout = {1, 0};

    struct event *recv_ev;
    recv_ev = event_new(ctx->base, ctx->sock, EV_READ|EV_PERSIST, on_value, ctx);
    struct event *monitor_ev;
    monitor_ev = event_new(ctx->base, -1, EV_TIMEOUT|EV_PERSIST, monitor, ctx);
    struct event *ev_sigterm;
    ev_sigterm = evsignal_new(ctx->base, SIGTERM, signal_handler, ctx);
    struct event *ev_sigint;
    ev_sigint = evsignal_new(ctx->base, SIGINT, signal_handler, ctx);

    event_base_priority_init(ctx->base, 4);
    event_priority_set(ev_sigint, 0);
    event_priority_set(ev_sigterm, 1);
    event_priority_set(monitor_ev, 2);
    event_priority_set(recv_ev, 3);

    event_add(ev_sigint, NULL);
    event_add(recv_ev, NULL);
    event_add(monitor_ev, &timeout);
    event_add(ev_sigterm, NULL);

    // Comment the line below for valgrind check
    event_base_dispatch(ctx->base);
    event_free(recv_ev);
    event_free(monitor_ev);
    event_free(ev_sigterm);
    event_free(ev_sigint);

    learner_ctx_destroy(ctx);

    return EXIT_SUCCESS;
}

int main(int argc, char* argv[]) {
    struct application *state = application_new();
    if (argc != 3) {
        printf("%s config-file node_id\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    Config *conf = parse_conf(argv[1]);
    conf->node_id = atoi(argv[2]);
    start_learner(conf, deliver, state);
    application_destroy(state);
    free(conf);
    return (EXIT_SUCCESS);
}