#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include "learner.h"
#include "config.h"
#include "netpaxos_utils.h"

struct application {
    int sock;
    int server_id;
    int number_of_servers;
    Config *conf;
} application;


int deliver(struct LearnerCtx *ctx, int inst, char* value, int size) {
    struct app_request *req = (struct app_request *) value;
    struct application *state = ctx->app;
    if (state->conf->verbose) {
        printf("instance %d: %s\n", inst, req->value);
    }
    // char res[] = "OK";
    int tp_dst = ntohs(req->client->sin_port);
    if ((tp_dst % state->number_of_servers) == state->server_id) {
        send_msg(state->sock, (char*)&tp_dst, 4, req->client);
    }
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        printf("%s config-file node_id number_of_servers\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    Config *conf = parse_conf(argv[1]);
    struct application *app = malloc(sizeof(struct application));
    app->server_id = atoi(argv[2]);
    app->number_of_servers = atoi(argv[3]);
    app->sock = create_socket();
    app->conf = conf;
    LearnerCtx *learner_ctx = make_learner(conf);
    set_app_ctx(learner_ctx, app);
    register_deliver_cb(learner_ctx, deliver);
    event_base_dispatch(learner_ctx->base);
    free_learner(learner_ctx);
    free(conf);
    free(app);
    return (EXIT_SUCCESS);
}