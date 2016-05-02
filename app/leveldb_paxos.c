#define _GNU_SOURCE
#include <leveldb/c.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "learner.h"
#include "config.h"
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

int deliver(struct LearnerCtx *ctx, int inst, struct app_request *req) {
    struct application *state = ctx->app;
    char *request = req->value;
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
            char *return_val = leveldb_get(state->db, state->roptions, &request[3], ksize, &read_len, &err);
            if (err != NULL) {
                leveldb_free(err); err = NULL;
                return FAILED;
            }
            if (*return_val) {
                // int return_vsize = read_len;
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