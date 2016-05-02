#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "learner.h"
#include "config.h"
#include "netpaxos_utils.h"

struct application {
    int sock;
} application;


int deliver(struct LearnerCtx *ctx, int inst, struct app_request *req) {
    struct application *state = ctx->app;
    // printf("delivered %s\n", req->value);
    char res[] = "OK";
    send_msg(state->sock, res, 2, req->client);
    return 0;
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        printf("%s config-file node_id\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    Config *conf = parse_conf(argv[1]);
    conf->node_id = atoi(argv[2]);
    struct application *app = malloc(sizeof(struct application));
    app->sock = create_socket();
    LearnerCtx *learner_ctx = make_learner(conf);
    set_app_ctx(learner_ctx, app);
    register_deliver_cb(learner_ctx, deliver);
    event_base_dispatch(learner_ctx->base);
    free_learner(learner_ctx);
    free(app);
    free(conf);
    return (EXIT_SUCCESS);
}