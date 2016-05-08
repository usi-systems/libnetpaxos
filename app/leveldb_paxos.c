#define _GNU_SOURCE
#include <leveldb/c.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
/* inet_ntoa */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
/* end inet_ntoa */
#include "learner.h"
#include "config.h"
#include "application.h"
#include "netpaxos_utils.h"

struct application {
    int sock;
    leveldb_t *db;
    leveldb_options_t *options;
    leveldb_readoptions_t *roptions;
    leveldb_writeoptions_t *woptions;
} application;

struct application* application_new() {
    struct application *state = malloc(sizeof(struct application));
    state->sock = create_socket();
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
    leveldb_writeoptions_destroy(state->woptions);
    leveldb_readoptions_destroy(state->roptions);
    leveldb_options_destroy(state->options);
    free(state);

    // leveldb_destroy_db(state->options, "/tmp/testdb", &err);
    // if (err != NULL) {
    //   fprintf(stderr, "Destroy fail.\n");
    //   return;
    // }
    // leveldb_free(err); err = NULL;

}

int deliver(struct LearnerCtx *ctx, int inst, char* value, int size) {
    struct app_request *reqctx = (struct app_request *) value;
    struct application *state = ctx->app;
    struct request *req = (struct request *)reqctx->value;
    size_t reqsize = sizeof(struct request);
    // printf("Received msgid: %d\n", *req->request_id);
    char *err = NULL;
    int my_turn = (inst % ctx->conf.num_acceptors == ctx->conf.node_id);

    size_t read_len;
    switch(req->op) {
        case PUT: {
            leveldb_put(state->db, state->woptions, req->key, 5, req->value, 6, &err);
            if (err != NULL) {
                leveldb_free(err); err = NULL;
                return FAILED;
            }
            if (my_turn) {
                send_msg(state->sock, (char*)req, reqsize, reqctx->client);
            }
            return SUCCESS;
        }
        case GET: {
            char *return_val = leveldb_get(state->db, state->roptions, req->key, 5, &read_len, &err);
            if (err != NULL) {
                leveldb_free(err); err = NULL;
                return FAILED;
            }
            if (*return_val) {
                // printf("GET value %s: %zu\n", return_val, read_len);
                memcpy(&req->value, return_val, read_len);
                if (my_turn) {
                    send_msg(state->sock, (char*)req, reqsize, reqctx->client);
                }
                free(return_val);
                return GOT_VALUE;
            }
            if (my_turn) {
                send_msg(state->sock, (char*)req, reqsize, reqctx->client);
            }
            return NOT_FOUND;
        }
        case DELETE: {
            leveldb_delete(state->db, state->woptions, req->key, 5, &err);
            if (err != NULL) {
                leveldb_free(err); err = NULL;
                return FAILED;
            }
            if (my_turn) {
                send_msg(state->sock, (char*)req, reqsize, reqctx->client);
            }
            return SUCCESS;
        }
    }
    // send_msg(state->sock, (char*)req_id, intsize, reqctx->client);
    return INVALID_OP;
}


int main(int argc, char* argv[]) {
    struct application *state = application_new();
    if (argc != 3) {
        printf("%s config-file node_id\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    Config *conf = parse_conf(argv[1]);
    conf->node_id = atoi(argv[2]);
    LearnerCtx *learner_ctx = make_learner(conf);
    set_app_ctx(learner_ctx, state);
    register_deliver_cb(learner_ctx, deliver);
    event_base_dispatch(learner_ctx->base);
    free_learner(learner_ctx);
    application_destroy(state);
    free(conf);
    return (EXIT_SUCCESS);
}