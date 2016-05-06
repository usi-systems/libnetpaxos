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
    struct app_request *req = (struct app_request *) value;
    struct application *state = ctx->app;
    char *request = req->value;
    int intsize = sizeof(int);
    int *req_id = (int *)req->value;
    // printf("Received msgid: %d\n", *req_id);
    char *err = NULL;
    int my_turn = (inst % ctx->conf.num_acceptors == ctx->conf.node_id);
    char op = request[intsize];

    // char *msg = req->value + 7;
    // printf("%d\n", op);
    // printf("req %s\n", msg);

    size_t read_len;
    switch(op) {
        case PUT: {
            unsigned char ksize = request[intsize + 1];
            unsigned char vsize = request[intsize + 2];

            // char key[ksize + 1];
            // memcpy(key, &request[intsize + 3], ksize);
            // key[ksize] = '\0';

            // char value[vsize + 1];
            // memcpy(value, &request[intsize + 3 + vsize], vsize);
            // printf("%s:%d, %s:%d\n", key, ksize,  value, vsize);

            leveldb_put(state->db, state->woptions, &request[intsize + 3], ksize, &request[intsize + 3 + ksize], vsize, &err);
            if (err != NULL) {
                leveldb_free(err); err = NULL;
                return FAILED;
            }
            if (my_turn) {
                send_msg(state->sock, (char*)req_id, intsize, req->client);
            }
            return SUCCESS;
        }
        case GET: {
            unsigned char ksize = request[intsize + 1];
            char *return_val = leveldb_get(state->db, state->roptions, &request[intsize + 3], ksize, &read_len, &err);
            if (err != NULL) {
                leveldb_free(err); err = NULL;
                return FAILED;
            }
            // char key[ksize + 1];
            // memcpy(key, &request[intsize + 3], ksize);
            // key[ksize] = '\0';
            // printf("GET key %s:%d\n", key, ksize);

            if (*return_val) {
                // printf("GET value %s: %zu\n", return_val, read_len);
                if (my_turn) {
                    send_msg(state->sock, (char*)req_id, read_len, req->client);
                }
                free(return_val);
                return GOT_VALUE;
            }
            if (my_turn) {
                send_msg(state->sock, (char*)req_id, intsize, req->client);
            }
            return NOT_FOUND;
        }
        case DELETE: {
            unsigned char ksize = request[intsize + 1];
            leveldb_delete(state->db, state->woptions, &request[intsize + 3], ksize, &err);
            if (err != NULL) {
                leveldb_free(err); err = NULL;
                return FAILED;
            }
            if (my_turn) {
                send_msg(state->sock, (char*)req_id, intsize, req->client);
            }
            return SUCCESS;
        }
    }
    // send_msg(state->sock, (char*)req_id, intsize, req->client);
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